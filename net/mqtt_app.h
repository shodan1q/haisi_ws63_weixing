/*
 * MQTT client for the dual-servo board. Config mirrors the esp32watch
 * project (bsp_mqtt.c):
 *   broker    tcp://121.41.23.138:1883
 *   client id weixing-a1
 *   username  public
 *   password  Aa123456
 *   sub topic sat/a1/cmd        (payload = target angle in degrees, ASCII int)
 *   pub topic sat/a1/telemetry  (JSON {"angle":N})
 */
#ifndef MQTT_APP_H
#define MQTT_APP_H

#include <stdbool.h>

/* Connect to the broker (WiFi must already be up). Subscribes to the
 * command topic. Returns 0 on success. */
int mqtt_app_connect(void);

/* Publish a telemetry JSON snapshot of the current servo angle. */
int mqtt_app_publish_telemetry(int angle_deg);

/* True once connected. */
bool mqtt_app_is_connected(void);

#endif
