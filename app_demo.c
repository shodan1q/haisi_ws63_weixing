/*
 * 4T_HRM_QS2 (Hi3863 / WS63) NearLink (SLE) provisioning demo
 *
 *   - Brings up SLE server (advertises as "SLE_DISTRIBUTE_SERVER", waits for
 *     a HarmonyOS client to pair / write the magic flag).
 *   - In parallel, connects to a hardcoded Wi-Fi AP so the board itself is
 *     online. Edit WIFI_SSID / WIFI_KEY below to match your AP.
 *   - LCD shows status. LEDs / buzzer / buttons are dropped to keep the
 *     provisioning demo focused; re-enable from git history if needed.
 *
 * To make this a real "phone sends Wi-Fi creds via SLE" flow, modify
 * `example_network_info_write_request_cbk()` in sle_server/src/...Server.c
 * so it parses the incoming bytes as an example_wifi_ssid_key_ntf_ind_t and
 * calls example_sta_function() with the received SSID / key.
 */

#include <stdio.h>
#include <string.h>
#include "common_def.h"
#include "errcode.h"
#include "soc_osal.h"
#include "app_init.h"
#include "osal_debug.h"
#include "lcd.h"
#include "sle_server/inc/SLE_Distribute_Network_Server.h"

/* ---- Wi-Fi credentials (edit before flashing) ---- */
#define WIFI_SSID  "your_ssid_here"
#define WIFI_KEY   "your_password_here"

/* ---- task config ---- */
#define TASK_STACK_SIZE      0x1000
#define WIFI_TASK_STACK_SIZE 0x2000
#define LCD_TASK_PRIO        26
#define WIFI_TASK_PRIO       20
#define LCD_REFRESH_MS       500

/* Imported from wifi_sta.c (copied verbatim from HiHope client side). */
extern errcode_t example_sta_function(const char *ssid, uint8_t ssid_len,
                                      const char *key,  uint8_t key_len);

/* shared status reported to LCD */
static volatile int g_wifi_state = 0;  /* 0=connecting, 1=ok, 2=fail */
static char         g_wifi_ip[24] = "";

/* =========================================================
 *  WiFi STA task
 * ========================================================= */
static void *wifi_task(const char *arg)
{
    unused(arg);

    osal_printk("[wifi] starting STA to SSID=%s\r\n", WIFI_SSID);
    g_wifi_state = 0;

    errcode_t ret = example_sta_function(WIFI_SSID, (uint8_t)(strlen(WIFI_SSID) + 1),
                                         WIFI_KEY,  (uint8_t)(strlen(WIFI_KEY) + 1));
    if (ret == ERRCODE_SUCC) {
        g_wifi_state = 1;
        osal_printk("[wifi] connected\r\n");
        /* example_sta_function already printed the IP; we leave g_wifi_ip
         * blank — LCD just reports CONNECTED. (Capturing the IP would
         * require modifying example_sta_function to return it.) */
    } else {
        g_wifi_state = 2;
        osal_printk("[wifi] failed, ret=0x%x\r\n", ret);
    }
    return NULL;
}

/* =========================================================
 *  LCD task
 * ========================================================= */
static void *lcd_task(const char *arg)
{
    unused(arg);
    char header[]   = "WS63 NearLink Cfg";
    char sle_hint[] = "SLE: SLE_DISTRIBUTE_SERVER";
    char sle_info[] = "  scan with HarmonyOS";
    char counter[32];

    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)header);
    spi_lcd_display_string_line(0, 2, WHITE, BLACK, (uint8_t *)sle_hint);
    spi_lcd_display_string_line(0, 3, WHITE, BLACK, (uint8_t *)sle_info);

    int last_wifi_state = -1;
    uint32_t tick = 0;

    for (;;) {
        if (g_wifi_state != last_wifi_state) {
            last_wifi_state = g_wifi_state;
            char blank[] = "                       ";
            spi_lcd_display_string_line(0, 5, WHITE, BLACK, (uint8_t *)blank);
            const char *msg;
            uint16_t color;
            switch (g_wifi_state) {
                case 1:  msg = "WiFi: CONNECTED";    color = GREEN; break;
                case 2:  msg = "WiFi: FAILED";       color = RED;   break;
                default: msg = "WiFi: connecting..."; color = WHITE; break;
            }
            spi_lcd_display_string_line(0, 5, color, BLACK, (uint8_t *)msg);
        }
        tick++;
        snprintf(counter, sizeof(counter), "Uptime: %lu s",
                 (unsigned long)(tick * LCD_REFRESH_MS / 1000));
        spi_lcd_display_string_line(0, 7, WHITE, BLACK, (uint8_t *)counter);
        osal_msleep(LCD_REFRESH_MS);
    }
    return NULL;
}

/* =========================================================
 *  Entry
 * ========================================================= */
static void app_entry(void)
{
    osal_task *h;

    osal_kthread_lock();

    h = osal_kthread_create((osal_kthread_handler)lcd_task, NULL, "LcdTask", TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, LCD_TASK_PRIO);

    h = osal_kthread_create((osal_kthread_handler)wifi_task, NULL, "WifiTask", WIFI_TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, WIFI_TASK_PRIO);

    osal_kthread_unlock();

    /* SLE server creates its own task internally with a 5s startup delay. */
    sle_provisioning_server_start();
}

app_run(app_entry);
