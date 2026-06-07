/*
 * MQTT client for the temp/humidity/pressure sensor board.
 * Request-response only — the board does NOT publish unless someone asks.
 *
 * Topics:
 *   SUBSCRIBE  cmnd/ws63_sensor/get        — any payload triggers a one-shot
 *                                             publish of current readings
 *   PUBLISH    stat/ws63_sensor/SENSOR     — JSON {"temp":..,"humid":..,"press":..}
 *              stat/ws63_sensor/temperature — "25.6"
 *              stat/ws63_sensor/humidity    — "65.3"
 *              stat/ws63_sensor/pressure    — "1013.2"
 *   All published messages are NON-retained, so subscribing alone never
 *   delivers data — you must explicitly request it with cmnd/.../get.
 */
#ifndef SENSORS_MQTT_H
#define SENSORS_MQTT_H

#include <stdbool.h>

/* Connect to the broker. WiFi must already be up. Returns 0 on success. */
int sensors_mqtt_connect(void);

bool sensors_mqtt_is_connected(void);

/* Callback the MQTT layer asks for fresh readings when cmnd/.../get arrives. */
typedef int (*sensors_mqtt_snapshot_cb_t)(float *t, float *h, float *p);
void sensors_mqtt_set_snapshot_cb(sensors_mqtt_snapshot_cb_t cb);

#endif
