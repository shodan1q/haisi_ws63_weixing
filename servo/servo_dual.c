/*
 * Dual SG90 servo bit-bang on GPIO_8 + GPIO_9.
 * Both pins toggle together every 20 ms with the same pulse width, so the
 * two servos stay mechanically in sync.
 */
#include "pinctrl.h"
#include "gpio.h"
#include "systick.h"
#include "soc_osal.h"
#include "watchdog.h"
#include "app_init.h"

#include "servo_dual.h"

#define SERVO_A_PIN      8           /* GPIO_8 */
#define SERVO_B_PIN      9           /* GPIO_9 */
#define PERIOD_US        20000       /* 20 ms = 50 Hz */

#define US_FOR_MIN       500         /* -90° */
#define US_FOR_CENTER    1500        /*   0° */
#define US_FOR_MAX       2500        /* +90° */

#define SWEEP_STEP_US    10          /* per 20 ms frame ≈ 0.9°, ~2 s for 90° */

#define SERVO_TASK_PRIO  17
#define SERVO_TASK_STACK 0x1000

volatile int g_servo_angle = 0;

static volatile unsigned int g_target_us  = US_FOR_CENTER;
static unsigned int          g_current_us = US_FOR_CENTER;

static int us_to_angle(unsigned int us)
{
    return ((int)us - (int)US_FOR_CENTER) * 90 / (int)(US_FOR_MAX - US_FOR_CENTER);
}

static unsigned int angle_to_us(int deg)
{
    if (deg < -90) deg = -90;
    if (deg >  90) deg =  90;
    return (unsigned int)(US_FOR_CENTER + deg * (int)(US_FOR_MAX - US_FOR_CENTER) / 90);
}

/* Emit one ~20 ms frame driving BOTH pins with the same pulse width.
 *
 * The HIGH pulse (0.5–2.5 ms) is a precise busy-wait — that's what the servo
 * actually decodes. The LOW remainder (~17.5–19.5 ms) is done with
 * osal_msleep so the scheduler can run the WiFi / MQTT / LCD tasks. Without
 * this yield the servo task (higher priority, never blocking) starves
 * everything else and WiFi never connects. The exact period drifting a bit
 * past 20 ms is fine — servos only care about the HIGH width. */
static void emit_frame(unsigned int high_us)
{
    uapi_gpio_set_val(SERVO_A_PIN, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(SERVO_B_PIN, GPIO_LEVEL_HIGH);
    uapi_systick_delay_us(high_us);
    uapi_gpio_set_val(SERVO_A_PIN, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(SERVO_B_PIN, GPIO_LEVEL_LOW);
    osal_msleep((PERIOD_US - high_us) / 1000);   /* ~18 ms, yields CPU */
}

void servo_set_angle(int angle_deg)
{
    g_target_us = angle_to_us(angle_deg);
}

static void servo_init_pins(void)
{
    uapi_pin_set_mode(SERVO_A_PIN, HAL_PIO_FUNC_GPIO);
    uapi_pin_set_mode(SERVO_B_PIN, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(SERVO_A_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_dir(SERVO_B_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(SERVO_A_PIN, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(SERVO_B_PIN, GPIO_LEVEL_LOW);
}

static void *servo_task(const char *arg)
{
    unused(arg);
    servo_init_pins();

    /* --- Reset: drive both servos to center for ~0.6 s --- */
    g_current_us = US_FOR_CENTER;
    g_target_us  = US_FOR_CENTER;
    for (int i = 0; i < 30; i++) {     /* 30 × 20 ms = 600 ms */
        emit_frame(g_current_us);
    }
    g_servo_angle = 0;
    osal_printk("[servo] reset to center done\r\n");

    /* --- Continuous refresh + smooth sweep toward target --- */
    for (;;) {
        uapi_watchdog_kick();
        if (g_current_us < g_target_us) {
            unsigned int d = g_target_us - g_current_us;
            g_current_us += (d < SWEEP_STEP_US) ? d : SWEEP_STEP_US;
        } else if (g_current_us > g_target_us) {
            unsigned int d = g_current_us - g_target_us;
            g_current_us -= (d < SWEEP_STEP_US) ? d : SWEEP_STEP_US;
        }
        emit_frame(g_current_us);
        g_servo_angle = us_to_angle(g_current_us);
    }
    return NULL;
}

void servo_dual_start(void)
{
    osal_task *h;
    osal_kthread_lock();
    h = osal_kthread_create((osal_kthread_handler)servo_task, NULL,
                            "ServoDual", SERVO_TASK_STACK);
    if (h) osal_kthread_set_priority(h, SERVO_TASK_PRIO);
    osal_kthread_unlock();
}
