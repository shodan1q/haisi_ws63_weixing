/*
 * Paho MQTT client for the WS63 sensor board (SHT30 + BMP280).
 *
 * Request-response design: the board only publishes after receiving a
 * cmnd/ws63_sensor/get message. Subscribers that just sit on tele/.../#
 * will see nothing — they have to ask. Published messages are NOT
 * retained, so the broker doesn't hold stale data either.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MQTTClient.h"
#include "soc_osal.h"
#include "common_def.h"
#include "osal_debug.h"

#include "sensors_mqtt.h"

/* ---- broker (same as esp32watch / servo board) ------------------------- */
#define MQTT_ADDRESS    "tcp://121.41.23.138:1883"
#define MQTT_USERNAME   "public"
#define MQTT_PASSWORD   "Aa123456"
/* Unique id; watch uses "weixing-a1", servo board uses "weixing-ws63". */
#define MQTT_CLIENTID   "ws63-sensor-a1"
#define MQTT_QOS        0

/* ---- topic root ------------------------------------------------------- */
#define DEV_ID          "ws63_sensor"
#define TOPIC_RESP_JSON "stat/" DEV_ID "/SENSOR"
#define TOPIC_RESP_T    "stat/" DEV_ID "/temperature"
#define TOPIC_RESP_H    "stat/" DEV_ID "/humidity"
#define TOPIC_RESP_P    "stat/" DEV_ID "/pressure"
#define TOPIC_CMD_GET   "cmnd/" DEV_ID "/get"

extern int MQTTClient_init(void);

static MQTTClient                 s_client;
static volatile bool              s_connected = false;
static sensors_mqtt_snapshot_cb_t s_snap_cb;

void sensors_mqtt_set_snapshot_cb(sensors_mqtt_snapshot_cb_t cb)
{
    s_snap_cb = cb;
}

static int publish_one(const char *topic, const char *payload)
{
    MQTTClient_message m = MQTTClient_message_initializer;
    MQTTClient_deliveryToken tok;
    m.payload    = (void *)payload;
    m.payloadlen = (int)strlen(payload);
    m.qos        = MQTT_QOS;
    m.retained   = 0;     /* never retain — strict request/response */
    return MQTTClient_publishMessage(s_client, topic, &m, &tok);
}

static void publish_snapshot(float t, float h, float p)
{
    char json[96];
    char buf[16];
    snprintf(json, sizeof(json),
             "{\"temp\":%d.%d,\"humid\":%d.%d,\"press\":%d.%d}",
             (int)t, (int)((t - (int)t) * 10),
             (int)h, (int)((h - (int)h) * 10),
             (int)p, (int)((p - (int)p) * 10));
    publish_one(TOPIC_RESP_JSON, json);

    snprintf(buf, sizeof(buf), "%d.%d", (int)t, (int)((t - (int)t) * 10));
    publish_one(TOPIC_RESP_T, buf);
    snprintf(buf, sizeof(buf), "%d.%d", (int)h, (int)((h - (int)h) * 10));
    publish_one(TOPIC_RESP_H, buf);
    snprintf(buf, sizeof(buf), "%d.%d", (int)p, (int)((p - (int)p) * 10));
    publish_one(TOPIC_RESP_P, buf);
    osal_printk("[mqtt] -> %s %s\r\n", TOPIC_RESP_JSON, json);
}

static void on_conn_lost(void *context, char *cause)
{
    unused(context);
    osal_printk("[mqtt] connection lost: %s\r\n", cause ? cause : "(null)");
    s_connected = false;
}

static int on_msg_arrived(void *context, char *topic_name, int topic_len,
                          MQTTClient_message *message)
{
    unused(context);
    unused(topic_len);

    osal_printk("[mqtt] cmd on %s\r\n", topic_name);

    /* Only thing we subscribe to is cmnd/.../get. Any payload triggers
     * a fresh snapshot publish to stat/.../SENSOR. */
    if (s_snap_cb) {
        float t = 0, h = 0, p = 0;
        if (s_snap_cb(&t, &h, &p) == 0) {
            publish_snapshot(t, h, p);
        }
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);
    return 1;
}

int sensors_mqtt_connect(void)
{
    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_init();
    rc = MQTTClient_create(&s_client, MQTT_ADDRESS, MQTT_CLIENTID,
                           MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("[mqtt] create failed rc=%d\r\n", rc);
        return -1;
    }

    opts.keepAliveInterval = 60;
    opts.cleansession      = 1;
    opts.username          = MQTT_USERNAME;
    opts.password          = MQTT_PASSWORD;

    MQTTClient_setCallbacks(s_client, NULL, on_conn_lost, on_msg_arrived, NULL);

    rc = MQTTClient_connect(s_client, &opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("[mqtt] connect failed rc=%d\r\n", rc);
        return -2;
    }
    osal_printk("[mqtt] connected to %s as %s\r\n", MQTT_ADDRESS, MQTT_CLIENTID);

    if (MQTTClient_subscribe(s_client, TOPIC_CMD_GET, MQTT_QOS) == MQTTCLIENT_SUCCESS) {
        osal_printk("[mqtt] subscribed %s\r\n", TOPIC_CMD_GET);
    }

    s_connected = true;
    return 0;
}

bool sensors_mqtt_is_connected(void)
{
    return s_connected;
}
