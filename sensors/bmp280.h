#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>
#include "i2c_bb.h"

/* BMP280 supports two addresses depending on SDO pin: 0x76 (SDO=GND) or 0x77.
 * The most common breakout modules tie it to 0x76. */
#define BMP280_I2C_ADDR  0x76

typedef struct {
    /* Calibration coefficients read once from NVM at init. */
    uint16_t dig_T1; int16_t dig_T2; int16_t dig_T3;
    uint16_t dig_P1; int16_t dig_P2; int16_t dig_P3;
    int16_t dig_P4; int16_t dig_P5; int16_t dig_P6;
    int16_t dig_P7; int16_t dig_P8; int16_t dig_P9;
} bmp280_calib_t;

/* Read chip ID (should be 0x58 for BMP280), load calibration coefficients,
 * configure for indoor normal mode (osrs_t=2, osrs_p=16, standby=500ms,
 * IIR filter x16). Returns 0 on success. */
int bmp280_init(const i2c_bb_t *bus, bmp280_calib_t *cal);

/* Read a fresh measurement.
 *   *temp_c       — temperature in °C (float)
 *   *pressure_hpa — pressure in hPa  (float)
 * Returns 0 on success. */
int bmp280_read(const i2c_bb_t *bus, const bmp280_calib_t *cal,
                float *temp_c, float *pressure_hpa);

#endif
