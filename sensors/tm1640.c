/*
 * TM1640 bit-bang driver. Adapted from the user-supplied ESP32 reference
 * (sx_tm1640.c). Two GPIOs in push-pull output mode.
 */
#include "pinctrl.h"
#include "gpio.h"
#include "systick.h"

#include "tm1640.h"

#define CMD_DATA              0x40    /* + 0x00 normal mode, auto-increment */
#define CMD_ADDRESS           0xC0    /* + offset 0..15 */
#define CMD_DISPLAY           0x80    /* + 0x08 on, + brightness 0..7 */
#define DATA_AUTOINC          0x00
#define DISPLAY_ON            0x08

#define TM1640_BIT_DELAY_US   2       /* TM1640 max clock ~1 MHz, plenty of margin */

static inline void clk_lo(const tm1640_t *d) { uapi_gpio_set_val(d->clk_pin,  GPIO_LEVEL_LOW); }
static inline void clk_hi(const tm1640_t *d) { uapi_gpio_set_val(d->clk_pin,  GPIO_LEVEL_HIGH); }
static inline void dat_lo(const tm1640_t *d) { uapi_gpio_set_val(d->data_pin, GPIO_LEVEL_LOW); }
static inline void dat_hi(const tm1640_t *d) { uapi_gpio_set_val(d->data_pin, GPIO_LEVEL_HIGH); }
static inline void dat_set(const tm1640_t *d, int v) { uapi_gpio_set_val(d->data_pin, v ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW); }
static inline void hold(void) { uapi_systick_delay_us(TM1640_BIT_DELAY_US); }

static void tm1640_start(const tm1640_t *d) { dat_lo(d); hold(); clk_lo(d); hold(); }
static void tm1640_stop(const tm1640_t *d)  { dat_lo(d); hold(); clk_hi(d); hold(); dat_hi(d); hold(); }

static void tm1640_send_byte(const tm1640_t *d, uint8_t v)
{
    for (int i = 0; i < 8; i++) {           /* LSB first per datasheet */
        dat_set(d, (v >> i) & 1);
        hold();
        clk_hi(d); hold();
        clk_lo(d); hold();
    }
}

void tm1640_init(const tm1640_t *dev)
{
    uapi_pin_set_mode(dev->clk_pin,  HAL_PIO_FUNC_GPIO);
    uapi_pin_set_mode(dev->data_pin, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(dev->clk_pin,  GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_dir(dev->data_pin, GPIO_DIRECTION_OUTPUT);
    clk_hi(dev);
    dat_hi(dev);
    hold();
}

void tm1640_set_brightness(const tm1640_t *dev, uint8_t value)
{
    if (value > 8) value = 8;
    tm1640_start(dev);
    if (value == 0) {
        tm1640_send_byte(dev, CMD_DISPLAY);                 /* display off */
    } else {
        tm1640_send_byte(dev, CMD_DISPLAY | DISPLAY_ON | (value - 1));
    }
    tm1640_stop(dev);
}

void tm1640_write(const tm1640_t *dev, const uint8_t *seg, uint8_t len, uint8_t offset)
{
    if (offset + len > 16) return;

    /* Data control: normal mode, auto-increment */
    tm1640_start(dev);
    tm1640_send_byte(dev, CMD_DATA | DATA_AUTOINC);
    tm1640_stop(dev);

    /* Address + payload */
    tm1640_start(dev);
    tm1640_send_byte(dev, CMD_ADDRESS | (offset & 0x0F));
    for (uint8_t i = 0; i < len; i++) {
        tm1640_send_byte(dev, seg[i]);
    }
    tm1640_stop(dev);
}

/* Standard 7-seg patterns. Bit map:
 *      a
 *    f   b
 *      g
 *    e   c
 *      d   dp
 *
 *  bits: dp g f e d c b a   (bit7..bit0)
 */
static uint8_t ascii_to_seg(char c)
{
    switch (c) {
        case '0': return 0x3F;
        case '1': return 0x06;
        case '2': return 0x5B;
        case '3': return 0x4F;
        case '4': return 0x66;
        case '5': return 0x6D;
        case '6': return 0x7D;
        case '7': return 0x07;
        case '8': return 0x7F;
        case '9': return 0x6F;
        case '-': return 0x40;
        case 'C': return 0x39;
        case 'H': return 0x76;
        case 'F': return 0x71;
        case 'P': return 0x73;
        case 'E': return 0x79;
        case 'r': return 0x50;
        case ' ': return 0x00;
        default:  return 0x00;
    }
}

void tm1640_show_text(const tm1640_t *dev, const char *text)
{
    uint8_t buf[16] = {0};
    uint8_t pos = 0;
    for (const char *p = text; *p && pos < 16; p++) {
        if (*p == '.') {
            if (pos > 0) buf[pos - 1] |= 0x80;
        } else {
            buf[pos++] = ascii_to_seg(*p);
        }
    }
    tm1640_write(dev, buf, pos, 0);
}
