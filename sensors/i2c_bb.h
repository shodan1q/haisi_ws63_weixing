/*
 * Bit-bang I2C master for WS63.
 *
 * Software I2C on any two GPIOs — no pin-mux research needed.
 * Slow enough (~50 kHz) for SHT30, BMP280, AT24Cxx etc., which is exactly
 * the speed range we care about. Open-drain semantics emulated with
 * GPIO direction switching (input = release line, output-low = pull down).
 */
#ifndef I2C_BB_H
#define I2C_BB_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t sda_pin;   /* GPIO number for SDA */
    uint8_t scl_pin;   /* GPIO number for SCL */
} i2c_bb_t;

/* Configure both pins for open-drain operation, drive lines high (idle). */
void i2c_bb_init(const i2c_bb_t *bus);

/* Write `len` bytes to 7-bit `addr`. Returns 0 on success, -1 on NAK. */
int i2c_bb_write(const i2c_bb_t *bus, uint8_t addr, const uint8_t *data, uint32_t len);

/* Read `len` bytes from 7-bit `addr`. Returns 0 on success, -1 on NAK. */
int i2c_bb_read(const i2c_bb_t *bus, uint8_t addr, uint8_t *data, uint32_t len);

/* Write `wlen` bytes then repeated-start and read `rlen` bytes.
 * Typical "read register" pattern: write reg pointer, read register data. */
int i2c_bb_write_read(const i2c_bb_t *bus, uint8_t addr,
                      const uint8_t *wbuf, uint32_t wlen,
                      uint8_t *rbuf, uint32_t rlen);

#endif
