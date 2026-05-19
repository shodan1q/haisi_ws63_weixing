/*
 * 4T_HRM_QS2 (Hi3863 / WS63) PN532 NFC test
 *
 * Wires:
 *   PN532  TXD --- GPIO_27 (board UART1 RX)
 *   PN532  RXD --- GPIO_26 (board UART1 TX)
 *   PN532  VCC --- 3.3V or 5V (check your module's jumpers)
 *   PN532  GND --- GND
 *
 * Make sure your PN532 module is jumpered for UART mode (most have a SET0/
 * SET1 pair: UART = SET0=0, SET1=0; default of most boards).
 *
 * What this demo does:
 *   1. Initialize PN532 (SAMConfiguration)
 *   2. Print firmware version to serial
 *   3. Poll for ISO14443-A cards every ~300 ms
 *   4. On hit: show UID hex on LCD, increment counter
 *   5. (optional) Mifare read/write available via pn532.h API
 */

#include <stdio.h>
#include <string.h>
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"
#include "lcd.h"

#include "nfc/pn532.h"

#define LCD_TASK_STACK_SIZE   0x1000
#define NFC_TASK_STACK_SIZE   0x1000
#define LCD_TASK_PRIO         26
#define NFC_TASK_PRIO         22
#define LCD_REFRESH_MS        200
#define NFC_POLL_INTERVAL_MS  300

/* Shared status. */
volatile int g_nfc_state = 0;      /* 0=init, 1=ready, 10=init fail */
volatile uint32_t g_nfc_hits  = 0;
volatile uint32_t g_nfc_polls = 0;   /* total polls since READY */
volatile int      g_nfc_last_rc = -999; /* last pn532_read_card_uid rc */
static char g_uid_str[40] = "(none)";
static char g_fw_str[20]  = "(?)";

static const char *nfc_state_str(int s)
{
    switch (s) {
        case 0:  return "PN532: initializing";
        case 1:  return "PN532: READY       ";
        case 10: return "PN532 INIT FAILED  ";
        default: return "PN532: ???         ";
    }
}

static void uid_to_hex(const uint8_t *uid, uint8_t len, char *out, int out_sz)
{
    int written = 0;
    for (uint8_t i = 0; i < len && written + 4 < out_sz; i++) {
        written += snprintf(out + written, out_sz - written, "%02X ", uid[i]);
    }
    if (written > 0) out[written - 1] = '\0';   /* drop trailing space */
}

static void *nfc_task(const char *arg)
{
    unused(arg);
    osal_msleep(500);  /* let LCD task draw the header first */

    if (pn532_init() != 0) {
        osal_printk("[nfc] pn532_init failed\r\n");
        g_nfc_state = 10;
        return NULL;
    }

    uint8_t fw[4];
    if (pn532_get_firmware_version(fw) == 0) {
        osal_printk("[nfc] firmware: IC=%02X Ver=%d.%d Support=%02X\r\n",
                    fw[0], fw[1], fw[2], fw[3]);
        snprintf(g_fw_str, sizeof(g_fw_str), "FW: %02X v%d.%d",
                 fw[0], fw[1], fw[2]);
    } else {
        snprintf(g_fw_str, sizeof(g_fw_str), "FW: unknown");
    }

    g_nfc_state = 1;

    uint8_t uid[PN532_MAX_UID_LEN];
    uint8_t uid_len = 0;
    char    last_uid[40] = "";

    uint32_t since_last_log = 0;
    for (;;) {
        int rc = pn532_read_card_uid(uid, &uid_len);
        g_nfc_polls++;
        g_nfc_last_rc = rc;

        if (rc == 0 && uid_len > 0) {
            char hex[40];
            uid_to_hex(uid, uid_len, hex, sizeof(hex));
            if (strcmp(hex, last_uid) != 0) {
                strncpy(last_uid, hex, sizeof(last_uid) - 1);
                snprintf(g_uid_str, sizeof(g_uid_str), "%s", hex);
                g_nfc_hits++;
                /* big banner so you can't miss it in serial */
                osal_printk("\r\n>>>>>>>>>> CARD DETECTED  UID(%u) = %s <<<<<<<<<<\r\n",
                            uid_len, hex);
            }
        } else {
            last_uid[0] = '\0';
        }

        /* Periodic heartbeat so you know the loop is alive. */
        since_last_log += NFC_POLL_INTERVAL_MS;
        if (since_last_log >= 3000) {
            since_last_log = 0;
            osal_printk("[nfc] polls=%lu hits=%lu last_rc=%d\r\n",
                        (unsigned long)g_nfc_polls,
                        (unsigned long)g_nfc_hits,
                        g_nfc_last_rc);
        }

        osal_msleep(NFC_POLL_INTERVAL_MS);
    }
    return NULL;
}

static void *lcd_task(const char *arg)
{
    unused(arg);

    spi_lcd_init();
    spi_lcd_clear(BLACK);
    char header[] = "WS63 PN532 NFC";
    char pins[]   = "UART1 GPIO_26/27";
    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)header);
    spi_lcd_display_string_line(0, 1, WHITE, BLACK, (uint8_t *)pins);

    int last_state = -1;
    uint32_t last_hits = 0xFFFFFFFFu;
    uint32_t last_polls = 0xFFFFFFFFu;
    char counter[32];
    char uid_line[40];
    char hits_line[24];
    char poll_line[32];

    uint32_t tick = 0;
    for (;;) {
        int s = g_nfc_state;
        if (s != last_state) {
            last_state = s;
            uint16_t color = (s == 1) ? GREEN : (s == 10 ? RED : WHITE);
            spi_lcd_display_string_line(0, 3, color, BLACK,
                                        (uint8_t *)nfc_state_str(s));
            if (s == 1) {
                spi_lcd_display_string_line(0, 4, WHITE, BLACK, (uint8_t *)g_fw_str);
            }
        }

        /* Live UID and counter. */
        snprintf(uid_line, sizeof(uid_line), "UID: %s         ", g_uid_str);
        spi_lcd_display_string_line(0, 5, WHITE, BLACK, (uint8_t *)uid_line);
        if (g_nfc_hits != last_hits) {
            last_hits = g_nfc_hits;
            snprintf(hits_line, sizeof(hits_line), "Hits: %lu       ",
                     (unsigned long)last_hits);
            spi_lcd_display_string_line(0, 6, GREEN, BLACK, (uint8_t *)hits_line);
        }
        /* Show poll counter + last rc so we know NFC task is alive. */
        if (g_nfc_polls != last_polls) {
            last_polls = g_nfc_polls;
            snprintf(poll_line, sizeof(poll_line),
                     "Polls:%lu rc=%d   ",
                     (unsigned long)last_polls, g_nfc_last_rc);
            spi_lcd_display_string_line(0, 9, WHITE, BLACK, (uint8_t *)poll_line);
        }

        tick++;
        snprintf(counter, sizeof(counter), "Uptime: %lu s",
                 (unsigned long)(tick * LCD_REFRESH_MS / 1000));
        spi_lcd_display_string_line(0, 7, WHITE, BLACK, (uint8_t *)counter);
        osal_msleep(LCD_REFRESH_MS);
    }
    return NULL;
}

static void app_entry(void)
{
    osal_task *h;
    osal_kthread_lock();

    h = osal_kthread_create((osal_kthread_handler)lcd_task, NULL,
                            "LcdTask", LCD_TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, LCD_TASK_PRIO);

    h = osal_kthread_create((osal_kthread_handler)nfc_task, NULL,
                            "NfcTask", NFC_TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, NFC_TASK_PRIO);

    osal_kthread_unlock();
}

app_run(app_entry);
