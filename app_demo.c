/*
 * 4T_HRM_QS2 (Hi3863 / WS63) NearLink (SLE) provisioning demo
 *
 * Two-board flow (per HiHope original design):
 *   - SERVER board advertises as SLE_DISTRIBUTE_SERVER, has hardcoded
 *     WIFI_SSID/WIFI_KEY, sends them to any peer that writes the trigger
 *     flag. It also connects WiFi itself using those creds.
 *   - CLIENT board scans for SERVER, connects, writes "WIFI_SSID_KEY"
 *     trigger, receives creds via notify, then connects WiFi.
 *
 * ─── pick the role for this build ────────────────────────────────────────
 */

#define WS63_ROLE_SERVER 1
#define WS63_ROLE_CLIENT 2

/* CHANGE THIS LINE PER BOARD: */
#define WS63_ROLE  WS63_ROLE_SERVER
/* ─────────────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <string.h>
#include "common_def.h"
#include "errcode.h"
#include "soc_osal.h"
#include "app_init.h"
#include "osal_debug.h"
#include "lcd.h"

#if WS63_ROLE == WS63_ROLE_SERVER
#include "sle_server/inc/SLE_Distribute_Network_Server.h"
#else
#include "sle_client/inc/SLE_Distribute_Network_Client.h"
#endif

/* ---- Wi-Fi credentials (SERVER role only — edit before flashing) ---- */
#define WIFI_SSID  "your_ssid_here"
#define WIFI_KEY   "your_password_here"

/* ---- payload buffer sizes ---- */
#define PROV_MAX_SSID_LEN  33
#define PROV_MAX_KEY_LEN   65

/* ---- task config ---- */
#define TASK_STACK_SIZE      0x1000
#define WIFI_TASK_STACK_SIZE 0x2000
#define LCD_TASK_PRIO        26
#define WIFI_TASK_PRIO       20
#define LCD_REFRESH_MS       500

/* From wifi_sta.c */
extern errcode_t example_sta_function(const char *ssid, uint8_t ssid_len,
                                      const char *key,  uint8_t key_len);

#if WS63_ROLE == WS63_ROLE_SERVER
extern volatile int g_sle_state;       /* server state */
static volatile int g_wifi_state = -1; /* -1 idle, 0 connecting, 1 ok, 2 fail */
static volatile int g_creds_ready = 0;
static char         g_received_ssid[PROV_MAX_SSID_LEN] = {0};
static char         g_received_key[PROV_MAX_KEY_LEN]   = {0};
static uint8_t      g_received_ssid_len = 0;
static uint8_t      g_received_key_len  = 0;

void sle_provisioning_creds_received(const char *ssid, uint8_t ssid_len,
                                     const char *key,  uint8_t key_len)
{
    if (ssid_len == 0 || ssid_len > PROV_MAX_SSID_LEN ||
        key_len  == 0 || key_len  > PROV_MAX_KEY_LEN) {
        osal_printk("[prov] bad lengths ssid=%u key=%u\r\n", ssid_len, key_len);
        return;
    }
    memcpy(g_received_ssid, ssid, ssid_len);
    memcpy(g_received_key,  key,  key_len);
    g_received_ssid_len = ssid_len;
    g_received_key_len  = key_len;
    g_creds_ready = 1;
    osal_printk("[prov] creds stored: ssid=%s\r\n", g_received_ssid);
}
#endif /* SERVER */

#if WS63_ROLE == WS63_ROLE_SERVER
static const char *server_state_str(int s)
{
    switch (s) {
        case 0:  return "SLE: waiting...    ";
        case 1:  return "SLE: enabling...   ";
        case 2:  return "SLE: reg conn cbks ";
        case 3:  return "SLE: reg ssaps cbks";
        case 4:  return "SLE: add server    ";
        case 5:  return "SLE: setup adv     ";
        case 6:  return "SLE: awaiting cb...";
        case 7:  return "SLE: ADVERTISING   ";
        case 10: return "SLE FAIL: enable   ";
        case 11: return "SLE FAIL: cbks     ";
        case 12: return "SLE FAIL: ssaps    ";
        case 13: return "SLE FAIL: srv add  ";
        case 14: return "SLE FAIL: adv      ";
        default: return "SLE: unknown       ";
    }
}
#else
static const char *client_state_str(int s)
{
    switch (s) {
        case 0:  return "CLIENT: starting...";
        case 1:  return "CLIENT: scanning   ";
        case 3:  return "CLIENT: connected  ";
        case 4:  return "CLIENT: got creds  ";
        case 5:  return "CLIENT: WiFi conn..";
        case 7:  return "CLIENT: ALL OK     ";
        case 10: return "CLIENT FAIL: enable";
        case 14: return "CLIENT FAIL: WiFi  ";
        default: return "CLIENT: unknown    ";
    }
}
#endif

#if WS63_ROLE == WS63_ROLE_SERVER
static const char *wifi_state_str(int s)
{
    switch (s) {
        case -1: return "WiFi: waiting creds";
        case 0:  return "WiFi: connecting...";
        case 1:  return "WiFi: CONNECTED    ";
        case 2:  return "WiFi: FAILED       ";
        default: return "WiFi: unknown      ";
    }
}

static int is_placeholder_ssid(void)
{
    return strcmp(WIFI_SSID, "your_ssid_here") == 0;
}

static void *wifi_task(const char *arg)
{
    unused(arg);
    const char *ssid;
    const char *key;
    uint8_t ssid_len, key_len;

    while (g_sle_state != 7 && g_sle_state < 10) {
        osal_msleep(500);
    }

    if (is_placeholder_ssid()) {
        osal_printk("[wifi] no hardcoded SSID; waiting for SLE creds...\r\n");
        g_wifi_state = -1;
        while (!g_creds_ready) osal_msleep(500);
        ssid = g_received_ssid;
        key  = g_received_key;
        ssid_len = g_received_ssid_len;
        key_len  = g_received_key_len;
    } else {
        ssid = WIFI_SSID;
        key  = WIFI_KEY;
        ssid_len = (uint8_t)(strlen(WIFI_SSID) + 1);
        key_len  = (uint8_t)(strlen(WIFI_KEY)  + 1);
    }

    osal_printk("[wifi] starting STA to SSID=%s\r\n", ssid);
    g_wifi_state = 0;

    errcode_t ret = example_sta_function(ssid, ssid_len, key, key_len);
    g_wifi_state = (ret == ERRCODE_SUCC) ? 1 : 2;
    osal_printk("[wifi] result=0x%x\r\n", ret);
    return NULL;
}
#endif /* SERVER */

/* =========================================================
 *  LCD task
 * ========================================================= */
static void *lcd_task(const char *arg)
{
    unused(arg);

    spi_lcd_init();
    spi_lcd_clear(BLACK);

#if WS63_ROLE == WS63_ROLE_SERVER
    char header[] = "WS63 SLE Server";
    char hint[]   = "Name: SLE_DISTRIBUTE_SERVER";
#else
    char header[] = "WS63 SLE Client";
    char hint[]   = "Seek: SLE_DISTRIBUTE_SERVER";
#endif
    char counter[32];
    char ssid_line[32];

    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)header);
    spi_lcd_display_string_line(0, 1, WHITE, BLACK, (uint8_t *)hint);

    int last_sle = -999;
#if WS63_ROLE == WS63_ROLE_SERVER
    int last_creds_or_wifi = -999;
#endif
    uint32_t tick = 0;

    for (;;) {
#if WS63_ROLE == WS63_ROLE_SERVER
        int s = g_sle_state;
        if (s != last_sle) {
            last_sle = s;
            uint16_t color = (s == 7) ? GREEN : (s >= 10 ? RED : WHITE);
            spi_lcd_display_string_line(0, 3, color, BLACK, (uint8_t *)server_state_str(s));
        }
        int w = g_wifi_state;
        if (w != last_creds_or_wifi) {
            last_creds_or_wifi = w;
            uint16_t color = (w == 1) ? GREEN : (w == 2 ? RED : WHITE);
            spi_lcd_display_string_line(0, 5, color, BLACK, (uint8_t *)wifi_state_str(w));
        }
        if (g_creds_ready) {
            snprintf(ssid_line, sizeof(ssid_line), "SSID: %s", g_received_ssid);
            spi_lcd_display_string_line(0, 6, GREEN, BLACK, (uint8_t *)ssid_line);
        }
#else
        int s = g_sle_client_state;
        if (s != last_sle) {
            last_sle = s;
            uint16_t color = (s == 7) ? GREEN : (s >= 10 ? RED : WHITE);
            spi_lcd_display_string_line(0, 3, color, BLACK, (uint8_t *)client_state_str(s));
        }
        if (s >= 4) {
            snprintf(ssid_line, sizeof(ssid_line), "SSID: %s", sle_client_get_ssid());
            spi_lcd_display_string_line(0, 5, GREEN, BLACK, (uint8_t *)ssid_line);
        }
#endif
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

#if WS63_ROLE == WS63_ROLE_SERVER
    h = osal_kthread_create((osal_kthread_handler)wifi_task, NULL, "WifiTask", WIFI_TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, WIFI_TASK_PRIO);
#endif
    osal_kthread_unlock();

#if WS63_ROLE == WS63_ROLE_SERVER
    sle_provisioning_server_start();
#else
    sle_provisioning_client_start();
#endif
}

app_run(app_entry);
