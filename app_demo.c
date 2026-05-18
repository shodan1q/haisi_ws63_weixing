/*
 * 4T_HRM_QS2 (Hi3863 / WS63) two-board SLE throughput demo
 *
 * Uses the SDK official sle_speed_{server,client} samples (from
 * src/application/samples/bt/sle/) imported into our custom/ tree.
 * No protocol changes — known-good SDK code. The only thing this file
 * does is pick a role at compile time and show it on the LCD.
 *
 * ─── pick role per board ────────────────────────────────────────────────
 */

#define WS63_ROLE_SERVER 1
#define WS63_ROLE_CLIENT 2

/* CHANGE THIS PER BOARD: */
#define WS63_ROLE  WS63_ROLE_SERVER
/* ──────────────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"
#include "lcd.h"

#if WS63_ROLE == WS63_ROLE_SERVER
#include "sle_speed_server/inc/sle_speed_server.h"
#else
#include "sle_speed_client/inc/sle_speed_client.h"
#endif

#define TASK_STACK_SIZE   0x1000
#define LCD_TASK_PRIO     26
#define LCD_REFRESH_MS    500

static void *lcd_task(const char *arg)
{
    unused(arg);

#if WS63_ROLE == WS63_ROLE_SERVER
    char header[] = "WS63 SLE SPEED";
    char role[]   = "Role: SERVER";
    char info[]   = "Adv -> wait peer";
    uint16_t hcol = GREEN;
#else
    char header[] = "WS63 SLE SPEED";
    char role[]   = "Role: CLIENT";
    char info[]   = "Seek -> connect";
    uint16_t hcol = BLUE2;
#endif
    char counter[32];

    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, hcol,  BLACK, (uint8_t *)header);
    spi_lcd_display_string_line(0, 2, WHITE, BLACK, (uint8_t *)role);
    spi_lcd_display_string_line(0, 4, WHITE, BLACK, (uint8_t *)info);

    uint32_t tick = 0;
    for (;;) {
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
    h = osal_kthread_create((osal_kthread_handler)lcd_task, NULL, "LcdTask", TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, LCD_TASK_PRIO);
    osal_kthread_unlock();

#if WS63_ROLE == WS63_ROLE_SERVER
    sle_speed_server_entry();
#else
    sle_speed_client_entry();
#endif
}

app_run(app_entry);
