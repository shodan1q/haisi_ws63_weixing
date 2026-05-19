#ifndef SG90_CONTROL_H
#define SG90_CONTROL_H

/* Servo task — registered by app_demo.c. Cycles 0° → +90° → 0° → -90° → … */
void *servo_task(const char *arg);

/* Live state for LCD. */
extern volatile int g_servo_angle;       /* current target: -90 / 0 / +90 */
extern volatile uint32_t g_servo_moves;  /* incremented each move */

#endif
