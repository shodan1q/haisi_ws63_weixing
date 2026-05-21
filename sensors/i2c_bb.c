/*
 * Software I2C master — open-drain emulation via GPIO direction toggling.
 *
 * Direction = OUTPUT, value = 0  → line driven LOW
 * Direction = INPUT, (pull-up)   → line released to HIGH
 *
 * Each pin is configured as GPIO with no pull (relying on the external
 * pull-up resistors on the I2C bus — most sensor breakouts include them).
 * If you ever wire a bare sensor without pull-ups, enable internal pullup
 * via uapi_gpio_set_pullup if your SDK exposes it.
 */
#include "pinctrl.h"
#include "gpio.h"
#include "systick.h"
#include "soc_osal.h"

#include "i2c_bb.h"

#define I2C_BIT_DELAY_US   5    /* ~100 kHz nominal */

static inline void scl_lo(const i2c_bb_t *b) { uapi_gpio_set_dir(b->scl_pin, GPIO_DIRECTION_OUTPUT); uapi_gpio_set_val(b->scl_pin, GPIO_LEVEL_LOW); }
static inline void scl_hi(const i2c_bb_t *b) { uapi_gpio_set_dir(b->scl_pin, GPIO_DIRECTION_INPUT); }
static inline void sda_lo(const i2c_bb_t *b) { uapi_gpio_set_dir(b->sda_pin, GPIO_DIRECTION_OUTPUT); uapi_gpio_set_val(b->sda_pin, GPIO_LEVEL_LOW); }
static inline void sda_hi(const i2c_bb_t *b) { uapi_gpio_set_dir(b->sda_pin, GPIO_DIRECTION_INPUT); }

static inline int sda_read(const i2c_bb_t *b)
{
    return uapi_gpio_get_val(b->sda_pin) == GPIO_LEVEL_HIGH ? 1 : 0;
}

static inline void hold(void) { uapi_systick_delay_us(I2C_BIT_DELAY_US); }

void i2c_bb_init(const i2c_bb_t *bus)
{
    uapi_pin_set_mode(bus->sda_pin, HAL_PIO_FUNC_GPIO);
    uapi_pin_set_mode(bus->scl_pin, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_val(bus->sda_pin, GPIO_LEVEL_LOW);  /* pre-set 0; we only drive low */
    uapi_gpio_set_val(bus->scl_pin, GPIO_LEVEL_LOW);
    sda_hi(bus);  /* release */
    scl_hi(bus);
    hold();
}

static void i2c_start(const i2c_bb_t *b)
{
    sda_hi(b); scl_hi(b); hold();
    sda_lo(b); hold();
    scl_lo(b); hold();
}

static void i2c_stop(const i2c_bb_t *b)
{
    sda_lo(b); hold();
    scl_hi(b); hold();
    sda_hi(b); hold();
}

/* Returns 1 if slave NACK'd, 0 if ACK. */
static int i2c_write_byte(const i2c_bb_t *b, uint8_t v)
{
    for (int i = 7; i >= 0; i--) {
        if (v & (1u << i)) sda_hi(b); else sda_lo(b);
        hold();
        scl_hi(b); hold();
        scl_lo(b); hold();
    }
    /* Read ACK */
    sda_hi(b); hold();
    scl_hi(b); hold();
    int nack = sda_read(b);
    scl_lo(b); hold();
    return nack;
}

static uint8_t i2c_read_byte(const i2c_bb_t *b, int ack)
{
    uint8_t v = 0;
    sda_hi(b);
    for (int i = 7; i >= 0; i--) {
        hold();
        scl_hi(b); hold();
        if (sda_read(b)) v |= (1u << i);
        scl_lo(b);
    }
    /* Send ACK or NACK */
    if (ack) sda_lo(b); else sda_hi(b);
    hold();
    scl_hi(b); hold();
    scl_lo(b); hold();
    sda_hi(b);
    return v;
}

int i2c_bb_write(const i2c_bb_t *bus, uint8_t addr, const uint8_t *data, uint32_t len)
{
    i2c_start(bus);
    if (i2c_write_byte(bus, (uint8_t)(addr << 1))) { i2c_stop(bus); return -1; }
    for (uint32_t i = 0; i < len; i++) {
        if (i2c_write_byte(bus, data[i])) { i2c_stop(bus); return -1; }
    }
    i2c_stop(bus);
    return 0;
}

int i2c_bb_read(const i2c_bb_t *bus, uint8_t addr, uint8_t *data, uint32_t len)
{
    i2c_start(bus);
    if (i2c_write_byte(bus, (uint8_t)((addr << 1) | 1))) { i2c_stop(bus); return -1; }
    for (uint32_t i = 0; i < len; i++) {
        data[i] = i2c_read_byte(bus, (i < len - 1) ? 1 : 0);
    }
    i2c_stop(bus);
    return 0;
}

int i2c_bb_write_read(const i2c_bb_t *bus, uint8_t addr,
                      const uint8_t *wbuf, uint32_t wlen,
                      uint8_t *rbuf, uint32_t rlen)
{
    i2c_start(bus);
    if (i2c_write_byte(bus, (uint8_t)(addr << 1))) { i2c_stop(bus); return -1; }
    for (uint32_t i = 0; i < wlen; i++) {
        if (i2c_write_byte(bus, wbuf[i])) { i2c_stop(bus); return -1; }
    }
    /* Repeated start, switch to read */
    i2c_start(bus);
    if (i2c_write_byte(bus, (uint8_t)((addr << 1) | 1))) { i2c_stop(bus); return -1; }
    for (uint32_t i = 0; i < rlen; i++) {
        rbuf[i] = i2c_read_byte(bus, (i < rlen - 1) ? 1 : 0);
    }
    i2c_stop(bus);
    return 0;
}
