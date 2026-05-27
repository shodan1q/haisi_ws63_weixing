/*
 * Two SG90 servos on GPIO_8 and GPIO_9, driven synchronously.
 *
 * Both servos always receive the identical pulse width in the same 20 ms
 * frame, so they move together. Software bit-bang (WS63 HW PWM can't do
 * 50 Hz). A background task keeps refreshing the current angle so the
 * servos hold position; call servo_set_angle() to retarget — the task
 * sweeps there smoothly.
 */
#ifndef SERVO_DUAL_H
#define SERVO_DUAL_H

#include <stdint.h>

/* Spawn the servo refresh task. It resets both servos to center (0°) first,
 * then holds whatever servo_set_angle() last requested. */
void servo_dual_start(void);

/* Retarget both servos. angle clamped to [-90, +90] degrees.
 * The refresh task sweeps smoothly from the current angle. */
void servo_set_angle(int angle_deg);

/* Current commanded angle (updates as the smooth sweep progresses). */
extern volatile int g_servo_angle;

#endif
