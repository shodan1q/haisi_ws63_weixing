/*
 * 4T_HRM_QS2 (Hi3863 / WS63) dual-servo + MQTT controller
 *
 *   - Two SG90 servos on GPIO_8 and GPIO_9, driven synchronously.
 *     Both reset to center (0°) at boot, then follow MQTT commands.
 *   - WiFi STA (edit WIFI_SSID / WIFI_PWD below).
 *   - MQTT to the esp32watch broker (tcp://121.41.23.138:1883):
 *       subscribe sat/a1/cmd       — payload is target angle, e.g. "45"
 *       publish   sat/a1/telemetry — {"angle":N} every few seconds
 *
 * LCD shows WiFi / MQTT / servo state.
 */
#include <stdio.h>
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"
#include "watchdog.h"
#include "lcd.h"

#include "servo/servo_dual.h"
#include "net/wifi_connect.h"
#include "net/mqtt_app.h"

/* ---- Wi-Fi credentials — EDIT THESE before flashing ---- */
#define WIFI_SSID  "your_ssid_here"
#define WIFI_PWD   "your_password_here"

#define LCD_TASK_STACK   0x1000
#define NET_TASK_STACK   0x2000
#define LCD_TASK_PRIO    26
#define NET_TASK_PRIO    24
#define TELEMETRY_PERIOD_MS 3000

/* status for LCD */
volatile int g_wifi_ok = 0;   /* 0 connecting, 1 ok */
volatile int g_mqtt_ok = 0;   /* 0 connecting, 1 ok */

static void *net_task(const char *arg)
{
    unused(arg);

    osal_printk("[net] connecting WiFi SSID=%s ...\r\n", WIFI_SSID);
    if (wifi_connect(WIFI_SSID, WIFI_PWD) != 0) {
        osal_printk("[net] wifi_connect failed\r\n");
        g_wifi_ok = 0;
        return NULL;
    }
    g_wifi_ok = 1;
    osal_printk("[net] WiFi connected\r\n");

    /* MQTT — retry until it sticks. */
    while (mqtt_app_connect() != 0) {
        osal_printk("[net] mqtt connect retry in 3 s...\r\n");
        osal_msleep(3000);
    }
    g_mqtt_ok = 1;

    /* Telemetry loop. Commands arrive asynchronously via the MQTT callback,
     * which calls servo_set_angle() directly. */
    for (;;) {
        if (mqtt_app_is_connected()) {
            g_mqtt_ok = 1;
            mqtt_app_publish_telemetry(g_servo_angle);
        } else {
            g_mqtt_ok = 0;
            osal_printk("[net] mqtt dropped, reconnecting...\r\n");
            mqtt_app_connect();
        }
        osal_msleep(TELEMETRY_PERIOD_MS);
    }
    return NULL;
}

static void *lcd_task(const char *arg)
{
    unused(arg);
    char hdr[]  = "WS63 Dual Servo+MQTT";
    char pins[] = "Servo GPIO_8 / GPIO_9";
    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)hdr);
    spi_lcd_display_string_line(0, 1, WHITE, BLACK, (uint8_t *)pins);

    char wifi_line[28], mqtt_line[28], ang_line[28], up[28];
    uint32_t tick = 0;
    for (;;) {
        snprintf(wifi_line, sizeof(wifi_line), "WiFi: %s     ",
                 g_wifi_ok ? "CONNECTED" : "...");
        snprintf(mqtt_line, sizeof(mqtt_line), "MQTT: %s     ",
                 g_mqtt_ok ? "CONNECTED" : "...");
        snprintf(ang_line, sizeof(ang_line), "Angle: %+4d deg   ", g_servo_angle);
        spi_lcd_display_string_line(0, 3, g_wifi_ok ? GREEN : WHITE, BLACK, (uint8_t *)wifi_line);
        spi_lcd_display_string_line(0, 4, g_mqtt_ok ? GREEN : WHITE, BLACK, (uint8_t *)mqtt_line);
        spi_lcd_display_string_line(0, 5, WHITE, BLACK, (uint8_t *)ang_line);

        tick++;
        snprintf(up, sizeof(up), "Uptime: %lu s", (unsigned long)(tick * 300 / 1000));
        spi_lcd_display_string_line(0, 7, WHITE, BLACK, (uint8_t *)up);
        osal_msleep(300);
    }
    return NULL;
}

static void app_entry(void)
{
    uapi_watchdog_disable();

    /* Servos first: reset to center immediately on boot. */
    servo_dual_start();

    osal_task *h;
    osal_kthread_lock();
    h = osal_kthread_create((osal_kthread_handler)lcd_task, NULL, "LcdTask", LCD_TASK_STACK);
    if (h) osal_kthread_set_priority(h, LCD_TASK_PRIO);
    h = osal_kthread_create((osal_kthread_handler)net_task, NULL, "NetTask", NET_TASK_STACK);
    if (h) osal_kthread_set_priority(h, NET_TASK_PRIO);
    osal_kthread_unlock();
}

app_run(app_entry);
