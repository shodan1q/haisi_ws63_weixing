/*
 * SG90 / SG92R servo control on GPIO_10 — smooth sweep version.
 *
 * SG90 expects 50 Hz PWM (20 ms period). WS63 hardware PWM minimum
 * frequency is too high for that, so we bit-bang in software with
 * uapi_systick_delay_us. Each tiny step (default 10 µs pulse change)
 * is followed by one 20 ms cycle, giving a ~2 s sweep from 0° to 90°.
 *
 * Pulse width → angle (SG90 datasheet):
 *     500 us  →  -90° (right)
 *    1500 us  →    0° (center)
 *    2500 us  →  +90° (left)
 */

#include "pinctrl.h"
#include "common_def.h"
#include "soc_osal.h"
#include "gpio.h"
#include "systick.h"
#include "watchdog.h"
#include "app_init.h"

#include "sg90_control.h"

#define SERVO_PIN        10           /* GPIO_10 per board pin map */
#define PERIOD_US        20000        /* 20 ms = 50 Hz */

/* Pulse-width limits for the servo's mechanical range. */
#define US_FOR_RIGHT     500          /*  -90° */
#define US_FOR_CENTER    1500         /*    0° */
#define US_FOR_LEFT      2500         /* +90°  */

/* Sweep granularity.
 *   Smaller STEP = smoother but slower (one PWM cycle = 20 ms per step).
 *   10 µs ≈ 0.9°, so a full 90° (1000 µs) sweep takes about 2.0 s.
 *   Bump STEP up or down to change the feel. */
#define SWEEP_STEP_US    10
#define HOLD_AT_END_MS   600          /* dwell at each extreme */
#define HOLD_AT_MID_MS   300          /* dwell at center */

/* Live status for LCD. */
volatile int g_servo_angle = 0;       /* current commanded angle in degrees */
volatile uint32_t g_servo_moves = 0;  /* completed sweeps (round trips) */

/* Internal: where the servo is right now, in pulse-width microseconds. */
static unsigned int g_current_us = US_FOR_CENTER;

/* Convert pulse-width back to a degree label for the LCD. */
static int pulse_to_angle(unsigned int us)
{
    /* (us - 1500) / 1000 * 90  — keep integer math */
    return ((int)us - (int)US_FOR_CENTER) * 90 / (int)(US_FOR_LEFT - US_FOR_CENTER);
}

/* Emit a single 20 ms PWM cycle at the given pulse width. */
static void servo_send_pulse(unsigned int high_us)
{
    uapi_gpio_set_val(SERVO_PIN, GPIO_LEVEL_HIGH);
    uapi_systick_delay_us(high_us);
    uapi_gpio_set_val(SERVO_PIN, GPIO_LEVEL_LOW);
    uapi_systick_delay_us(PERIOD_US - high_us);
}

/* Hold the current pulse width for `ms` milliseconds, refreshing every cycle. */
static void servo_hold(unsigned int ms)
{
    /* one PWM cycle is 20 ms, so 50 cycles per second */
    unsigned int cycles = (ms + (PERIOD_US / 1000) - 1) / (PERIOD_US / 1000);
    for (unsigned int i = 0; i < cycles; i++) {
        servo_send_pulse(g_current_us);
    }
}

/* Smoothly sweep the pulse width from g_current_us to target_us, one
 * SWEEP_STEP_US per PWM cycle. Updates g_servo_angle as it goes so the
 * LCD reflects motion in near-real-time. */
static void servo_sweep_to(unsigned int target_us)
{
    int step = (target_us > g_current_us) ? SWEEP_STEP_US : -SWEEP_STEP_US;
    while (g_current_us != target_us) {
        /* don't overshoot on the last step */
        int remaining = (int)target_us - (int)g_current_us;
        if ((step > 0 && remaining < step) || (step < 0 && remaining > step)) {
            g_current_us = target_us;
        } else {
            g_current_us = (unsigned int)((int)g_current_us + step);
        }
        servo_send_pulse(g_current_us);
        g_servo_angle = pulse_to_angle(g_current_us);
    }
}

static void servo_init(void)
{
    uapi_pin_set_mode(SERVO_PIN, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(SERVO_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(SERVO_PIN, GPIO_LEVEL_LOW);
}

void *servo_task(const char *arg)
{
    unused(arg);
    servo_init();

    /* Snap to center on boot without a sweep (initial position unknown). */
    for (int i = 0; i < 10; i++) servo_send_pulse(US_FOR_CENTER);
    g_current_us = US_FOR_CENTER;
    g_servo_angle = 0;

    for (;;) {
        uapi_watchdog_kick();

        servo_sweep_to(US_FOR_LEFT);    /* 0°  -> +90° (smooth, ~2 s) */
        servo_hold(HOLD_AT_END_MS);

        servo_sweep_to(US_FOR_CENTER);  /* +90° -> 0°  (smooth, ~2 s) */
        servo_hold(HOLD_AT_MID_MS);

        servo_sweep_to(US_FOR_RIGHT);   /* 0°  -> -90° (smooth, ~2 s) */
        servo_hold(HOLD_AT_END_MS);

        servo_sweep_to(US_FOR_CENTER);  /* -90° -> 0°  (smooth, ~2 s) */
        servo_hold(HOLD_AT_MID_MS);

        g_servo_moves++;
    }
    return NULL;
}
