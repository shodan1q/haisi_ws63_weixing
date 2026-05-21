#ifndef SHT30_H
#define SHT30_H

#include <stdint.h>
#include "i2c_bb.h"

#define SHT30_I2C_ADDR  0x44

/* Trigger a one-shot measurement (high repeatability, clock stretching off)
 * and read 6 bytes (temp MSB, temp LSB, CRC, hum MSB, hum LSB, CRC).
 * Outputs:
 *   *temp_c  — degrees Celsius (float)
 *   *humid_p — relative humidity 0..100 (%)
 * Returns 0 on success, negative on I2C error, +1 if CRC mismatch. */
int sht30_read(const i2c_bb_t *bus, float *temp_c, float *humid_p);

#endif
