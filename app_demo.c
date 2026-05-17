/*
 * 4T_HRM_QS2 (Hi3863 / WS63) NearLink (SLE) provisioning demo
 *
 *   - Brings up SLE server, advertises as "SLE_DISTRIBUTE_SERVER".
 *   - LCD shows SLE init step + WiFi state without needing serial.
 *   - WiFi STA waits for SLE to be advertising before starting, and is
 *     skipped entirely if WIFI_SSID is still the placeholder string —
 *     this isolates SLE from WiFi radio activity while you debug.
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

/* ---- Wi-Fi credentials ----
 * Leave as-is to SKIP WiFi entirely (recommended while debugging SLE).
 * Replace with real SSID/key to also bring up WiFi after SLE comes up. */
#define WIFI_SSID  "your_ssid_here"
#define WIFI_KEY   "your_password_here"

/* ---- task config ---- */
#define TASK_STACK_SIZE      0x1000
#define WIFI_TASK_STACK_SIZE 0x2000
#define LCD_TASK_PRIO        26
#define WIFI_TASK_PRIO       20
#define LCD_REFRESH_MS       500

/* From wifi_sta.c */
extern errcode_t example_sta_function(const char *ssid, uint8_t ssid_len,
                                      const char *key,  uint8_t key_len);

/* From sle_server: 0/1/2/3/4/5 = init steps, 7 = OK, 10..14 = fail steps */
extern volatile int g_sle_state;

/* shared status reported to LCD */
static volatile int g_wifi_state = -1;  /* -1=skipped, 0=connecting, 1=ok, 2=fail */

static const char *sle_state_str(int s)
{
    switch (s) {
        case 0:  return "SLE: waiting...    ";
        case 1:  return "SLE: enabling...   ";
        case 2:  return "SLE: reg conn cbks ";
        case 3:  return "SLE: reg ssaps cbks";
        case 4:  return "SLE: add server    ";
        case 5:  return "SLE: setup adv     ";
        case 7:  return "SLE: ADVERTISING   ";
        case 10: return "SLE FAIL: enable   ";
        case 11: return "SLE FAIL: cbks     ";
        case 12: return "SLE FAIL: ssaps    ";
        case 13: return "SLE FAIL: srv add  ";
        case 14: return "SLE FAIL: adv      ";
        default: return "SLE: unknown       ";
    }
}

static const char *wifi_state_str(int s)
{
    switch (s) {
        case -1: return "WiFi: SKIPPED      ";
        case 0:  return "WiFi: connecting...";
        case 1:  return "WiFi: CONNECTED    ";
        case 2:  return "WiFi: FAILED       ";
        default: return "WiFi: unknown      ";
    }
}

/* =========================================================
 *  WiFi STA task
 * ========================================================= */
static void *wifi_task(const char *arg)
{
    unused(arg);

    if (strcmp(WIFI_SSID, "your_ssid_here") == 0) {
        osal_printk("[wifi] placeholder SSID, skipping WiFi\r\n");
        g_wifi_state = -1;
        return NULL;
    }

    /* Wait for SLE to finish initializing so the WiFi scan doesn't
     * starve the radio while SLE is bringing itself up. */
    osal_printk("[wifi] waiting for SLE ready before starting...\r\n");
    while (g_sle_state != 7 && g_sle_state < 10) {
        osal_msleep(500);
    }
    osal_msleep(1000);

    osal_printk("[wifi] starting STA to SSID=%s\r\n", WIFI_SSID);
    g_wifi_state = 0;

    errcode_t ret = example_sta_function(WIFI_SSID, (uint8_t)(strlen(WIFI_SSID) + 1),
                                         WIFI_KEY,  (uint8_t)(strlen(WIFI_KEY) + 1));
    if (ret == ERRCODE_SUCC) {
        g_wifi_state = 1;
        osal_printk("[wifi] connected\r\n");
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
    char header[]  = "WS63 NearLink Cfg";
    char hint[]    = "Name: SLE_DISTRIBUTE_SERVER";
    char counter[32];

    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)header);
    spi_lcd_display_string_line(0, 1, WHITE, BLACK, (uint8_t *)hint);

    int last_sle = -999, last_wifi = -999;
    uint32_t tick = 0;
    for (;;) {
        if (g_sle_state != last_sle) {
            last_sle = g_sle_state;
            uint16_t color = (last_sle == 7) ? GREEN
                           : (last_sle >= 10) ? RED : WHITE;
            spi_lcd_display_string_line(0, 3, color, BLACK,
                                        (uint8_t *)sle_state_str(last_sle));
        }
        if (g_wifi_state != last_wifi) {
            last_wifi = g_wifi_state;
            uint16_t color = (last_wifi == 1) ? GREEN
                           : (last_wifi == 2) ? RED : WHITE;
            spi_lcd_display_string_line(0, 5, color, BLACK,
                                        (uint8_t *)wifi_state_str(last_wifi));
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

    /* SLE server creates its own task internally. */
    sle_provisioning_server_start();
}

app_run(app_entry);
