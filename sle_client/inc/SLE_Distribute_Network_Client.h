/*
# Copyright (C) 2024 HiHope Open Source Organization .
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
 */

/**
 * @defgroup
 * @ingroup
 * @{
 */

#ifndef SLE_DISTRIBUTE_NETWORK_CLIENT_H
#define SLE_DISTRIBUTE_NETWORK_CLIENT_H

/* Start the SLE client provisioning task — scans for SLE_DISTRIBUTE_SERVER,
 * connects, writes the 13-byte trigger flag, receives the credentials via
 * notify, and connects WiFi. */
void sle_provisioning_client_start(void);

/* Observable state for LCD diagnostics:
 *   0 init, 1 enabled/scanning, 3 connected, 4 creds received,
 *   5 wifi connecting, 7 ALL OK, 10 enable_sle fail, 14 wifi fail */
extern volatile int g_sle_client_state;

/* Last received SSID (after state >= 4). Pointer to internal buffer. */
const char *sle_client_get_ssid(void);

#endif