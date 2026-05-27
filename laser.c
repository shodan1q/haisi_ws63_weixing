#include "pinctrl.h"
#include "gpio.h"
#include "osal_debug.h"

#include "laser.h"

#define LASER_PIN  10   /* GPIO_10 → transistor base */

static bool g_laser_on = false;

void laser_init(void)
{
    uapi_pin_set_mode(LASER_PIN, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(LASER_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(LASER_PIN, GPIO_LEVEL_LOW);
    g_laser_on = false;
}

void laser_set(bool on)
{
    uapi_gpio_set_val(LASER_PIN, on ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
    g_laser_on = on;
    osal_printk("[laser] %s\r\n", on ? "ON" : "OFF");
}

bool laser_get(void)
{
    return g_laser_on;
}
