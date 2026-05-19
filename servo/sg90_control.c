/*
 * SG90 / SG92R servo control on GPIO_10.
 *
 * SG90 expects 50 Hz PWM (20 ms period). The WS63 hardware PWM minimum
 * frequency is too high for that, so we bit-bang the signal in software
 * with uapi_systick_delay_us — same approach the HiHope vendor demo uses.
 *
 * Pulse width → angle (typical SG90):
 *     500 us  →  -90° (right)
 *    1500 us  →    0° (center)
 *    2500 us  →  +90° (left)
 *
 * Note: GPIO_10 is the red LED on 4T_HRM_QS2 — if the LED was wired by the
 * board you'll see it follow the servo pulse. Disconnect/repurpose as
 * needed. The servo signal pin connects to the same GPIO_10 header.
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
#define BURST_COUNT      10           /* send 10 pulses per move (≈200 ms) */

#define US_FOR_RIGHT     500          /*  -90° */
#define US_FOR_CENTER    1500         /*    0° */
#define US_FOR_LEFT      2500         /* +90°  */

/* Live status for LCD. */
volatile int g_servo_angle = 0;       /* -90 / 0 / +90 */
volatile uint32_t g_servo_moves = 0;  /* total move count */

static void servo_send_pulse(unsigned int high_us)
{
    uapi_gpio_set_val(SERVO_PIN, GPIO_LEVEL_HIGH);
    uapi_systick_delay_us(high_us);
    uapi_gpio_set_val(SERVO_PIN, GPIO_LEVEL_LOW);
    uapi_systick_delay_us(PERIOD_US - high_us);
}

static void servo_move_to(unsigned int high_us, int angle_label)
{
    for (int i = 0; i < BURST_COUNT; i++) {
        servo_send_pulse(high_us);
    }
    g_servo_angle = angle_label;
    g_servo_moves++;
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

    for (;;) {
        uapi_watchdog_kick();
        servo_move_to(US_FOR_CENTER, 0);    osal_msleep(800);
        servo_move_to(US_FOR_LEFT,  +90);   osal_msleep(800);
        servo_move_to(US_FOR_CENTER, 0);    osal_msleep(800);
        servo_move_to(US_FOR_RIGHT, -90);   osal_msleep(800);
    }
    return NULL;
}
