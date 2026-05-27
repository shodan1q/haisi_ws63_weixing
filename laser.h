/*
 * IR / laser emitter on GPIO_10, switched through an NPN transistor.
 * GPIO_10 HIGH  → transistor conducts → emitter ON
 * GPIO_10 LOW   → emitter OFF
 *
 * (GPIO_10 is the board's LED1 net, repurposed here. The servos moved to
 * GPIO_8/9 so there's no conflict.)
 */
#ifndef LASER_H
#define LASER_H

#include <stdbool.h>

void laser_init(void);
void laser_set(bool on);
bool laser_get(void);

#endif
