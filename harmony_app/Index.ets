/*
 * WS63 NearLink Provisioning — main page
 *
 * Drop this into `entry/src/main/ets/pages/Index.ets` (replacing the
 * default), plus copy NearLinkService.ets and ProvisioningPayload.ets to
 * `entry/src/main/ets/common/` (or any path you like — adjust the imports).
 */

import { NearLinkService, DiscoveredDevice } from '../common/NearLinkService';
import { ProvisioningPayload } from '../common/ProvisioningPayload';
import promptAction from '@ohos.promptAction';

@Entry
@Component
struct Index {
  @State devices: DiscoveredDevice[] = [];
  @State selectedAddr: string = '';
  @State ssid: string = '';
  @State password: string = '';
  @State isScanning: boolean = false;
  @State isConnected: boolean = false;
  @State isReady: boolean = false;            // service+property resolved
  @State sending: boolean = false;
  @State statusText: string = 'idle';

  private nl: NearLinkService = new NearLinkService();

  aboutToAppear() {
    this.nl.on('device', (d: DiscoveredDevice) => {
      // dedupe by address
      if (!this.devices.find(x => x.addr === d.addr)) {
        this.devices = [...this.devices, d];
      }
    });
    this.nl.on('state', (s: string) => {
      this.statusText = s;
      if (s === 'connected') this.isConnected = true;
      if (s.includes('disconnect') || s.includes('failed')) this.isConnected = false;
    });
    this.nl.on('ready', () => {
      this.isReady = true;
      this.statusText = 'ready — fill SSID/password and tap Send';
    });
    this.nl.on('result', (ok: boolean, msg: string) => {
      this.sending = false;
      this.statusText = ok ? 'creds sent — board will connect WiFi now' : `send failed: ${msg}`;
      promptAction.showToast({ message: this.statusText, duration: 3000 });
    });
  }

  aboutToDisappear() {
    this.nl.stopScan();
    this.nl.disconnect();
  }

  private async toggleScan() {
    if (this.isScanning) {
      await this.nl.stopScan();
      this.isScanning = false;
    } else {
      this.devices = [];
      this.statusText = 'scanning...';
      await this.nl.startScan();
      this.isScanning = true;
    }
  }

  private async connectTo(addr: string) {
    this.selectedAddr = addr;
    await this.nl.stopScan();
    this.isScanning = false;
    await this.nl.connect(addr);
  }

  private async sendCreds() {
    if (!this.ssid) {
      promptAction.showToast({ message: 'enter SSID' });
      return;
    }
    try {
      const payload = ProvisioningPayload.encode(this.ssid, this.password);
      this.sending = true;
      this.statusText = 'sending creds...';
      await this.nl.writeProvisioning(payload);
    } catch (e) {
      this.statusText = `encode failed: ${(e as Error).message}`;
      promptAction.showToast({ message: this.statusText });
    }
  }

  build() {
    Column({ space: 12 }) {
      Text('WS63 NearLink Provisioning')
        .fontSize(22)
        .fontWeight(FontWeight.Bold)
        .margin({ top: 16, bottom: 8 })

      Text(this.statusText)
        .fontSize(13)
        .fontColor('#666')
        .width('100%')
        .padding({ left: 16, right: 16 })

      Button(this.isScanning ? 'Stop Scan' : 'Scan for SLE_DISTRIBUTE_SERVER')
        .width('90%')
        .onClick(() => this.toggleScan())

      // Device list
      if (this.devices.length > 0) {
        List({ space: 6 }) {
          ForEach(this.devices, (d: DiscoveredDevice) => {
            ListItem() {
              Row() {
                Column() {
                  Text(d.name).fontSize(15).fontWeight(FontWeight.Medium)
                  Text(d.addr).fontSize(11).fontColor('#888')
                }.layoutWeight(1).alignItems(HorizontalAlign.Start)
                Text(`${d.rssi} dBm`).fontSize(12).fontColor('#888')
              }.padding(12).width('100%')
                .backgroundColor(d.addr === this.selectedAddr ? '#E0F0FF' : '#FAFAFA')
                .borderRadius(8)
            }.onClick(() => this.connectTo(d.addr))
          }, (d: DiscoveredDevice) => d.addr)
        }.width('90%').height(180)
      } else {
        Text(this.isScanning ? 'searching...' : 'no devices yet')
          .fontSize(13).fontColor('#999').height(60)
      }

      // Credentials form (enabled after services resolved)
      if (this.isReady) {
        TextInput({ placeholder: 'WiFi SSID' })
          .width('90%')
          .onChange((v: string) => this.ssid = v)
        TextInput({ placeholder: 'WiFi password' })
          .width('90%')
          .type(InputType.Password)
          .onChange((v: string) => this.password = v)
        Button(this.sending ? 'Sending...' : 'Send Credentials')
          .width('90%')
          .enabled(!this.sending && !!this.ssid)
          .onClick(() => this.sendCreds())
      }
    }
    .width('100%').height('100%')
    .alignItems(HorizontalAlign.Center)
  }
}
