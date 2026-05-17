/*
 * Thin wrapper over HarmonyOS NearLink Kit for the WS63 provisioning demo.
 *
 * NOTE — NearLink Kit's public API surface has been moving. This file
 * targets the namespace `@kit.NearLinkKit` (HarmonyOS NEXT, API 12+). If
 * your DevEco Studio uses different module names (e.g. an older
 * `@ohos.nearlink`), adjust the imports — the shape of the calls below is
 * representative of the standard discovery/connect/SSAP write flow.
 *
 * Required permissions in entry/src/main/module.json5:
 *
 *   "requestPermissions": [
 *     { "name": "ohos.permission.ACCESS_NEARLINK"        },
 *     { "name": "ohos.permission.APPROXIMATELY_LOCATION" }
 *   ]
 */

// Adjust these imports to whatever your DevEco Studio version exposes.
// All four are sub-modules of NearLinkKit in the typical layout.
import { access, scan, connection, ssap } from '@kit.NearLinkKit';
import { BusinessError } from '@ohos.base';
import hilog from '@ohos.hilog';

import {
  SLE_SERVICE_SHORT_UUID,
  SLE_PROPERTY_SHORT_UUID,
} from './ProvisioningPayload';

const TAG = 'WS63Prov';
const DOMAIN = 0xCAFE;

export interface DiscoveredDevice {
  name: string;
  addr: string;
  rssi: number;
}

/**
 * Event names emitted by NearLinkService.
 *   'device'   -> (d: DiscoveredDevice)  every scan hit
 *   'state'    -> (s: string)            connection state changes
 *   'ready'    -> ()                     services + property resolved, can write
 *   'result'   -> (ok: boolean, msg)     write result
 */
type Listener = (...args: ESObject[]) => void;

export class NearLinkService {
  private listeners: Map<string, Listener[]> = new Map();
  private deviceAddr: string = '';
  private serviceHandle: number = 0;
  private propertyHandle: number = 0;
  private scanning: boolean = false;

  on(event: string, cb: Listener) {
    if (!this.listeners.has(event)) this.listeners.set(event, []);
    this.listeners.get(event)!.push(cb);
  }

  private emit(event: string, ...args: ESObject[]) {
    this.listeners.get(event)?.forEach(cb => cb(...args));
  }

  /** Verify NearLink is powered on; ask the user to enable it otherwise. */
  async ensureEnabled(): Promise<boolean> {
    try {
      const state = access.getNearLinkState();   // method name may differ
      // HarmonyOS uses an enum like NearLinkState.STATE_ON
      if (state.toString().includes('ON')) {
        return true;
      }
      hilog.warn(DOMAIN, TAG, 'NearLink is off; ask user to enable in Settings');
      return false;
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'access.getNearLinkState failed: %{public}s',
                  (e as BusinessError).message);
      return false;
    }
  }

  /** Start scanning, filtering for our server name. */
  async startScan(): Promise<void> {
    if (this.scanning) return;
    if (!await this.ensureEnabled()) return;

    scan.on('deviceFound', (devs: ESObject[]) => {
      for (const raw of devs) {
        const name = (raw['deviceName'] ?? raw['name'] ?? '') as string;
        const addr = (raw['deviceAddr'] ?? raw['address'] ?? '') as string;
        const rssi = (raw['rssi'] ?? 0) as number;
        if (name.includes('SLE_DISTRIBUTE') && addr) {
          this.emit('device', { name, addr, rssi } as DiscoveredDevice);
        }
      }
    });

    try {
      // Most NearLink scan APIs accept a filter spec + mode.
      scan.startScan({ filters: [{ name: 'SLE_DISTRIBUTE_SERVER' }] });
      this.scanning = true;
      hilog.info(DOMAIN, TAG, 'scan started');
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'scan.startScan failed: %{public}s',
                  (e as BusinessError).message);
    }
  }

  async stopScan(): Promise<void> {
    if (!this.scanning) return;
    try {
      scan.stopScan();
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'scan.stopScan failed: %{public}s',
                  (e as BusinessError).message);
    }
    this.scanning = false;
  }

  /** Connect to a device by address and resolve our property handle. */
  async connect(addr: string): Promise<void> {
    await this.stopScan();
    this.deviceAddr = addr;
    this.emit('state', 'connecting');

    try {
      connection.on('connectionStateChange', (info: ESObject) => {
        const state = info['state'] ?? info['connectionState'];
        hilog.info(DOMAIN, TAG, 'connection state: %{public}s', String(state));
        if (String(state).includes('CONNECT') && String(state).includes('ED')) {
          this.emit('state', 'connected');
          this.discoverServices();
        } else if (String(state).includes('DISCONNECT')) {
          this.emit('state', 'disconnected');
        }
      });
      await connection.connect({ deviceAddr: addr });
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'connect failed: %{public}s',
                  (e as BusinessError).message);
      this.emit('state', 'connect failed');
    }
  }

  private async discoverServices(): Promise<void> {
    this.emit('state', 'discovering services');
    try {
      ssap.on('servicesDiscovered', (info: ESObject) => {
        const services = (info['services'] ?? []) as ESObject[];
        for (const svc of services) {
          const svcUuid = String(svc['uuid'] ?? '').toUpperCase();
          if (svcUuid.includes('ABCD')) {
            this.serviceHandle = (svc['handle'] ?? 0) as number;
            const props = (svc['properties'] ?? svc['characteristics'] ?? []) as ESObject[];
            for (const p of props) {
              const pUuid = String(p['uuid'] ?? '').toUpperCase();
              if (pUuid.includes('3344')) {
                this.propertyHandle = (p['handle'] ?? 0) as number;
                hilog.info(DOMAIN, TAG,
                  'matched WS63 service handle=%{public}d, property handle=%{public}d',
                  this.serviceHandle, this.propertyHandle);
                this.emit('ready');
                return;
              }
            }
          }
        }
        this.emit('state', 'WS63 service not found');
      });
      await ssap.discoverServices({ deviceAddr: this.deviceAddr });
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'discoverServices failed: %{public}s',
                  (e as BusinessError).message);
      this.emit('state', 'discover failed');
    }
  }

  /** Write the 117-byte provisioning payload to property 0x3344. */
  async writeProvisioning(payload: ArrayBuffer): Promise<void> {
    if (!this.propertyHandle) {
      this.emit('result', false, 'property handle not resolved yet');
      return;
    }
    try {
      await ssap.write({
        deviceAddr: this.deviceAddr,
        handle: this.propertyHandle,
        value: payload,
        // some versions of the API ask for write-type:
        writeType: 'WRITE_WITHOUT_RESPONSE',
      });
      this.emit('result', true, 'creds sent');
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'ssap.write failed: %{public}s',
                  (e as BusinessError).message);
      this.emit('result', false, (e as BusinessError).message);
    }
  }

  async disconnect(): Promise<void> {
    if (!this.deviceAddr) return;
    try {
      await connection.disconnect({ deviceAddr: this.deviceAddr });
    } catch (e) {
      // best-effort
    }
    this.deviceAddr = '';
    this.propertyHandle = 0;
    this.serviceHandle = 0;
  }
}
