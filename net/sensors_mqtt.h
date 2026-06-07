/*
 * MQTT client for the temp/humidity/pressure sensor board.
 * Reuses the same broker as the rest of this project (esp32watch's broker)
 * but with a unique client id so all three boards (watch/servo/sensor) can
 * be online at the same time.
 *
 * Topics (Tasmota-style):
 *   PUBLISH   tele/ws63_sensor/SENSOR        — JSON {"temp":..,"humid":..,"press":..}
 *             tele/ws63_sensor/temperature   — "25.6"   (each value individually)
 *             tele/ws63_sensor/humidity      — "65.3"
 *             tele/ws63_sensor/pressure      — "1013.2"
 *             stat/ws63_sensor/SENSOR        — JSON, posted on demand
 *   SUBSCRIBE cmnd/ws63_sensor/get           — any payload triggers an immediate
 *                                              publish to stat/.../SENSOR
 */
#ifndef SENSORS_MQTT_H
#define SENSORS_MQTT_H

#include <stdbool.h>

/* Connect to the broker. WiFi must already be up. Subscribes to the
 * "give me a reading now" command topic. Returns 0 on success. */
int sensors_mqtt_connect(void);

/* Publish a full snapshot to all sensor topics (periodic telemetry). */
int sensors_mqtt_publish_telemetry(float temperature_c, float humidity_p, float pressure_hpa);

bool sensors_mqtt_is_connected(void);

/* Called by the MQTT callback when a "get" command arrives — the
 * application provides current readings; the wrapper publishes them
 * to the stat/.../SENSOR topic. */
typedef int (*sensors_mqtt_snapshot_cb_t)(float *t, float *h, float *p);
void sensors_mqtt_set_snapshot_cb(sensors_mqtt_snapshot_cb_t cb);

#endif
