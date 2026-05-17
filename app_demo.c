/*
 * 4T_HRM_QS2 (Hi3863 / WS63) NearLink (SLE) provisioning demo
 *
 *   - SLE server advertises as "SLE_DISTRIBUTE_SERVER".
 *   - LCD shows SLE state + WiFi state.
 *   - Two ways to drive WiFi connection (auto-selected):
 *       a) hardcoded: set WIFI_SSID below — board connects on boot.
 *       b) phone-driven: leave WIFI_SSID as placeholder — board waits for
 *          a NearLink peer to write the 117-byte credentials payload to
 *          property UUID 0x3344, then connects with those creds.
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

/* ---- Wi-Fi credentials ---- */
#define WIFI_SSID  "your_ssid_here"      /* placeholder => phone-driven mode */
#define WIFI_KEY   "your_password_here"

/* ---- payload buffer sizes (match SLE server struct) ---- */
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

/* From sle_server: 0..5 init steps, 7 = OK, 10..14 = fail steps */
extern volatile int g_sle_state;

/* shared status for LCD */
static volatile int g_wifi_state = -1;  /* -1 idle/skipped, 0 connecting, 1 ok, 2 fail */
static volatile int g_creds_ready = 0;
static volatile int g_peer_connected = 0;  /* set true when phone is paired & writing */
static char         g_received_ssid[PROV_MAX_SSID_LEN] = {0};
static char         g_received_key[PROV_MAX_KEY_LEN]   = {0};
static uint8_t      g_received_ssid_len = 0;
static uint8_t      g_received_key_len  = 0;

/*
 * Called from the SLE write-request callback when a peer writes the full
 * 117-byte credentials struct. Runs in the SLE protocol stack thread, so we
 * just copy out the bytes and let wifi_task do the connect.
 */
void sle_provisioning_creds_received(const char *ssid, uint8_t ssid_len,
                                     const char *key,  uint8_t key_len)
{
    if (ssid_len == 0 || ssid_len > PROV_MAX_SSID_LEN ||
        key_len  == 0 || key_len  > PROV_MAX_KEY_LEN) {
        osal_printk("[prov] invalid lengths ssid=%u key=%u\r\n", ssid_len, key_len);
        return;
    }
    memcpy(g_received_ssid, ssid, ssid_len);
    memcpy(g_received_key,  key,  key_len);
    g_received_ssid_len = ssid_len;
    g_received_key_len  = key_len;
    g_peer_connected = 1;
    g_creds_ready = 1;
    osal_printk("[prov] creds stored: ssid=%s\r\n", g_received_ssid);
}

static const char *sle_state_str(int s)
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

/* =========================================================
 *  WiFi STA task
 * ========================================================= */
static void *wifi_task(const char *arg)
{
    unused(arg);
    const char *ssid;
    const char *key;
    uint8_t ssid_len, key_len;

    /* Wait for SLE to come up so the radio is settled. */
    while (g_sle_state != 7 && g_sle_state < 10) {
        osal_msleep(500);
    }

    if (is_placeholder_ssid()) {
        /* Phone-driven mode — wait until a NearLink peer writes creds. */
        osal_printk("[wifi] no hardcoded SSID; waiting for SLE provisioning...\r\n");
        g_wifi_state = -1;
        while (!g_creds_ready) {
            osal_msleep(500);
        }
        ssid     = g_received_ssid;
        key      = g_received_key;
        ssid_len = g_received_ssid_len;
        key_len  = g_received_key_len;
    } else {
        /* Hardcoded mode. */
        ssid     = WIFI_SSID;
        key      = WIFI_KEY;
        ssid_len = (uint8_t)(strlen(WIFI_SSID) + 1);
        key_len  = (uint8_t)(strlen(WIFI_KEY) + 1);
    }

    osal_printk("[wifi] starting STA to SSID=%s\r\n", ssid);
    g_wifi_state = 0;

    errcode_t ret = example_sta_function(ssid, ssid_len, key, key_len);
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
    char header[] = "WS63 NearLink Cfg";
    char hint[]   = "Name: SLE_DISTRIBUTE_SERVER";
    char ssid_line[32] = "";
    char counter[32];

    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)header);
    spi_lcd_display_string_line(0, 1, WHITE, BLACK, (uint8_t *)hint);

    int last_sle = -999, last_wifi = -999, last_creds = -1;
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
        if (g_creds_ready != last_creds) {
            last_creds = g_creds_ready;
            if (g_creds_ready) {
                snprintf(ssid_line, sizeof(ssid_line), "SSID: %s", g_received_ssid);
                spi_lcd_display_string_line(0, 6, GREEN, BLACK, (uint8_t *)ssid_line);
            }
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

    sle_provisioning_server_start();
}

app_run(app_entry);
