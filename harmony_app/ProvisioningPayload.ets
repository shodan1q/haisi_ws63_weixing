/*
 * 117-byte payload format for WS63 SLE provisioning.
 * Must match `example_wifi_ssid_key_ntf_ind_t` in
 *   sle_server/src/SLE_Distribute_Network_Server.c
 *
 *   offset  size  field
 *   ------  ----  --------------------------------
 *   0       17    flag       — "WIFI_SSID_KEY\0" + zero pad
 *   17      1     ssid_len   — strlen(ssid) + 1   (includes the trailing \0)
 *   18      1     key_len    — strlen(key)  + 1
 *   19      33    ssid       — null-terminated, zero-padded
 *   52      65    key        — null-terminated, zero-padded
 *   ----    ----
 *   total   117
 */

export const WIFI_SSID_KEY_FLAG = 'WIFI_SSID_KEY';
export const NTF_IND_FLAG_MAX_LEN = 17;
export const WIFI_MAX_SSID_LEN = 33;
export const WIFI_MAX_KEY_LEN = 65;
export const PROVISIONING_PAYLOAD_LEN =
  NTF_IND_FLAG_MAX_LEN + 1 + 1 + WIFI_MAX_SSID_LEN + WIFI_MAX_KEY_LEN; // 117

/* WS63 SLE server UUIDs (see sle_server/inc/SLE_Distribute_Network_Server.h). */
export const SLE_SERVICE_SHORT_UUID  = 0xABCD;
export const SLE_PROPERTY_SHORT_UUID = 0x3344;

/* Long-form UUIDs used by some HarmonyOS NearLink Kit APIs. The 16-byte base
 * is 37BEA880-FC70-11EA-B720-{2 short bytes}-00000000 (low-endian last 4 bytes
 * are zero in the HiSpark UUID encoding). */
export const SLE_SERVICE_UUID  = '37BEA880-FC70-11EA-B720-0000ABCD0000';
export const SLE_PROPERTY_UUID = '37BEA880-FC70-11EA-B720-000033440000';

export class ProvisioningPayload {
  /**
   * Encode an SSID/password pair into the 117-byte buffer the WS63 server
   * expects when writing to property 0x3344.
   *
   * Throws if ssid/password are too long for the protocol.
   */
  static encode(ssid: string, password: string): ArrayBuffer {
    const enc = new TextEncoder();
    const ssidBytes = enc.encode(ssid);
    const keyBytes  = enc.encode(password);

    if (ssidBytes.length + 1 > WIFI_MAX_SSID_LEN) {
      throw new Error(`SSID too long (max ${WIFI_MAX_SSID_LEN - 1} bytes)`);
    }
    if (keyBytes.length + 1 > WIFI_MAX_KEY_LEN) {
      throw new Error(`Password too long (max ${WIFI_MAX_KEY_LEN - 1} bytes)`);
    }

    const buf = new Uint8Array(PROVISIONING_PAYLOAD_LEN);
    // already zero-initialized

    // flag
    const flagBytes = enc.encode(WIFI_SSID_KEY_FLAG);
    buf.set(flagBytes, 0);                                  // bytes 0..12
    // bytes 13..16 stay 0 (null + padding)

    // ssid_len and key_len: include the trailing \0
    buf[NTF_IND_FLAG_MAX_LEN]     = ssidBytes.length + 1;   // byte 17
    buf[NTF_IND_FLAG_MAX_LEN + 1] = keyBytes.length + 1;    // byte 18

    // ssid
    buf.set(ssidBytes, NTF_IND_FLAG_MAX_LEN + 2);           // bytes 19..

    // key
    buf.set(keyBytes,
            NTF_IND_FLAG_MAX_LEN + 2 + WIFI_MAX_SSID_LEN);  // bytes 52..

    return buf.buffer;
  }

  /** Hex dump of an ArrayBuffer — handy for debugging. */
  static hex(buf: ArrayBuffer): string {
    const a = new Uint8Array(buf);
    return Array.from(a).map(b => b.toString(16).padStart(2, '0')).join(' ');
  }
}
