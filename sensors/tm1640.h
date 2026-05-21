/*
 * TM1640 7-segment driver — Titan Micro Electronics two-wire LED driver.
 *
 * Note: TM1640 wires are LABELED "DATA / CLK" or sometimes "SDA / SCL" on
 * cheap modules, but this is NOT real I2C. It's a custom serial protocol:
 *   - LSB first
 *   - No address byte / no ACK bit
 *   - Frame = START + COMMAND or DATA + STOP
 *
 * Commands (TM1640 datasheet):
 *   0x40 | flags  Data control: 0x40 = normal + auto-increment, 0x44 = fixed addr
 *   0xC0 | offs   Address command, offs 0..15 (we have 16-byte display SRAM)
 *   0x80 | flags  Display control: bit 3 = on/off, bits 0..2 = brightness 0..7
 *                  e.g., 0x88 = on, brightness 1; 0x8F = on, brightness 8
 */
#ifndef TM1640_H
#define TM1640_H

#include <stdint.h>

/* Pin assignment. Set before tm1640_init(). */
typedef struct {
    uint8_t clk_pin;   /* GPIO for CLK (SCK) */
    uint8_t data_pin;  /* GPIO for DIN (SDA) */
} tm1640_t;

void tm1640_init(const tm1640_t *dev);

/* brightness 0..8; 0 = display off. */
void tm1640_set_brightness(const tm1640_t *dev, uint8_t value);

/* Write `len` 7-segment bytes into SRAM starting at `offset`. */
void tm1640_write(const tm1640_t *dev, const uint8_t *seg, uint8_t len, uint8_t offset);

/* Convenience: render up to 8 ASCII digits onto SRAM. Position 0 is leftmost.
 * Supports '0'..'9', '.', '-', ' ', 'C', 'H', 'F'. A '.' is OR-ed into the
 * preceding digit (no separate position consumed). */
void tm1640_show_text(const tm1640_t *dev, const char *text);

#endif
