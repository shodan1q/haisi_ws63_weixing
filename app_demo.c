/*
 * 4T_HRM_QS2 (Hi3863 / WS63) SG90 servo demo
 *
 * Spins up two tasks:
 *   - servo_task : bit-bangs 50 Hz PWM on GPIO_10 to drive the servo
 *                  (HiHope-style, the WS63 HW PWM can't reach 50 Hz)
 *   - lcd_task   : shows the current commanded angle + move counter
 */

#include <stdio.h>
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"
#include "lcd.h"

#include "servo/sg90_control.h"

#define LCD_TASK_STACK_SIZE      0x1000
#define SERVO_TASK_STACK_SIZE    0x1000
#define LCD_TASK_PRIO            26
#define SERVO_TASK_PRIO          17
#define LCD_REFRESH_MS           200

static uint16_t angle_color(int a)
{
    if (a >=  60) return RED;     /* near +90 */
    if (a <= -60) return BLUE;    /* near -90 */
    if (a >= -10 && a <= 10) return GREEN;   /* near center */
    return WHITE;                  /* mid-sweep */
}

static void *lcd_task(const char *arg)
{
    unused(arg);
    char header[] = "WS63 SG90 servo";
    char pin[]    = "Pin: GPIO_10 (PWM)";
    char counter[32];
    char moves[32];
    char angle_line[32];

    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)header);
    spi_lcd_display_string_line(0, 1, WHITE, BLACK, (uint8_t *)pin);

    int last_angle = -999;
    uint32_t last_moves = 0xFFFFFFFFu;
    uint32_t tick = 0;

    for (;;) {
        int a = g_servo_angle;
        if (a != last_angle) {
            last_angle = a;
            /* extra trailing spaces wipe leftover digits when shrinking */
            snprintf(angle_line, sizeof(angle_line), "ANGLE: %+4d deg    ", a);
            spi_lcd_display_string_line(0, 3, angle_color(a), BLACK,
                                        (uint8_t *)angle_line);
        }
        if (g_servo_moves != last_moves) {
            last_moves = g_servo_moves;
            snprintf(moves, sizeof(moves), "Moves: %lu    ",
                     (unsigned long)last_moves);
            spi_lcd_display_string_line(0, 5, WHITE, BLACK, (uint8_t *)moves);
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

    h = osal_kthread_create((osal_kthread_handler)servo_task, NULL,
                            "ServoTask", SERVO_TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, SERVO_TASK_PRIO);

    osal_kthread_unlock();
}

app_run(app_entry);
