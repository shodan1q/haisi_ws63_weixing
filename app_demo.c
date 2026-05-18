/*
 * 4T_HRM_QS2 (Hi3863 / WS63) two-board SLE throughput demo
 *
 * Uses the SDK official sle_speed_{server,client} samples imported into
 * our custom/ tree. LCD shows connection state in real time.
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
#define LCD_REFRESH_MS    300

#if WS63_ROLE == WS63_ROLE_SERVER
static const char *server_state_str(int s)
{
    switch (s) {
        case 0:  return "STATE: advertising ";
        case 1:  return "STATE: CONNECTED   ";
        case 2:  return "STATE: disconnected";
        default: return "STATE: unknown     ";
    }
}
#else
static const char *client_state_str(int s)
{
    switch (s) {
        case 0:  return "STATE: scanning    ";
        case 1:  return "STATE: server FOUND";
        case 2:  return "STATE: CONNECTED   ";
        case 3:  return "STATE: disconnected";
        default: return "STATE: unknown     ";
    }
}
#endif

static void *lcd_task(const char *arg)
{
    unused(arg);

#if WS63_ROLE == WS63_ROLE_SERVER
    char header[] = "WS63 SLE SPEED";
    char role[]   = "Role: SERVER";
    uint16_t hcol = GREEN;
#else
    char header[] = "WS63 SLE SPEED";
    char role[]   = "Role: CLIENT";
    uint16_t hcol = BLUE2;
#endif
    char counter[32];
    char peer_line[32];
#if WS63_ROLE == WS63_ROLE_CLIENT
    char extra_line[32];
#endif

    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, hcol,  BLACK, (uint8_t *)header);
    spi_lcd_display_string_line(0, 2, WHITE, BLACK, (uint8_t *)role);

    int last_state = -1;
    uint32_t tick = 0;

    for (;;) {
#if WS63_ROLE == WS63_ROLE_SERVER
        int s = g_server_link_state;
        if (s != last_state) {
            last_state = s;
            uint16_t color = (s == 1) ? GREEN : (s == 2 ? RED : WHITE);
            spi_lcd_display_string_line(0, 3, color, BLACK,
                                        (uint8_t *)server_state_str(s));
            if (s == 1 || s == 2) {
                snprintf(peer_line, sizeof(peer_line), "Peer: %s", g_server_peer_addr);
                spi_lcd_display_string_line(0, 4, color, BLACK, (uint8_t *)peer_line);
            }
        }
#else
        int s = g_client_link_state;
        if (s != last_state) {
            last_state = s;
            uint16_t color = (s == 2) ? GREEN : (s == 3 ? RED : WHITE);
            spi_lcd_display_string_line(0, 3, color, BLACK,
                                        (uint8_t *)client_state_str(s));
            if (s >= 1 && s <= 2) {
                snprintf(peer_line, sizeof(peer_line), "Srv: %s", g_client_peer_addr);
                spi_lcd_display_string_line(0, 4, color, BLACK, (uint8_t *)peer_line);
            }
        }
        snprintf(extra_line, sizeof(extra_line), "RX pkts: %lu     ",
                 (unsigned long)g_client_recv_pkts);
        spi_lcd_display_string_line(0, 5, WHITE, BLACK, (uint8_t *)extra_line);
#endif
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
