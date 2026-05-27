/*
 * Paho MQTTClient wrapper. Connects to the esp32watch broker, subscribes
 * to the command topic, and drives the servos when a command arrives.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MQTTClient.h"
#include "soc_osal.h"
#include "common_def.h"
#include "osal_debug.h"

#include "mqtt_app.h"
#include "../servo/servo_dual.h"

/* ===== config copied from esp32watch/firmware/main/bsp_mqtt.c ===== */
#define MQTT_ADDRESS    "tcp://121.41.23.138:1883"
#define MQTT_CLIENTID   "weixing-a1"
#define MQTT_USERNAME   "public"
#define MQTT_PASSWORD   "Aa123456"
#define TOPIC_CMD       "sat/a1/cmd"
#define TOPIC_TELEMETRY "sat/a1/telemetry"
#define MQTT_QOS        0

extern int MQTTClient_init(void);

static MQTTClient s_client;
static volatile bool s_connected = false;

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

    /* Copy payload to a NUL-terminated local buffer. */
    char buf[32] = {0};
    int n = message->payloadlen;
    if (n > (int)sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, message->payload, n);
    buf[n] = '\0';

    osal_printk("[mqtt] cmd on %s = '%s'\r\n", topic_name, buf);

    /* Command payload is the target angle in degrees (e.g. "45", "-90"). */
    int angle = atoi(buf);
    servo_set_angle(angle);
    osal_printk("[mqtt] servo target set to %d deg\r\n", angle);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);
    return 1;   /* message handled */
}

int mqtt_app_connect(void)
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_init();
    rc = MQTTClient_create(&s_client, MQTT_ADDRESS, MQTT_CLIENTID,
                           MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("[mqtt] create failed rc=%d\r\n", rc);
        return -1;
    }

    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.username = MQTT_USERNAME;
    conn_opts.password = MQTT_PASSWORD;

    MQTTClient_setCallbacks(s_client, NULL, on_conn_lost, on_msg_arrived, NULL);

    rc = MQTTClient_connect(s_client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("[mqtt] connect failed rc=%d\r\n", rc);
        return -2;
    }
    osal_printk("[mqtt] connected to %s as %s\r\n", MQTT_ADDRESS, MQTT_CLIENTID);

    rc = MQTTClient_subscribe(s_client, TOPIC_CMD, MQTT_QOS);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("[mqtt] subscribe %s failed rc=%d\r\n", TOPIC_CMD, rc);
    } else {
        osal_printk("[mqtt] subscribed %s\r\n", TOPIC_CMD);
    }

    s_connected = true;
    return 0;
}

int mqtt_app_publish_telemetry(int angle_deg)
{
    if (!s_connected) return -1;

    char payload[48];
    snprintf(payload, sizeof(payload), "{\"angle\":%d}", angle_deg);

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = MQTT_QOS;
    pubmsg.retained = 0;

    int rc = MQTTClient_publishMessage(s_client, TOPIC_TELEMETRY, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        osal_printk("[mqtt] publish failed rc=%d\r\n", rc);
        return -2;
    }
    return 0;
}

bool mqtt_app_is_connected(void)
{
    return s_connected;
}
