/*
 * BMP280 driver — talks via the bit-bang I2C bus.
 * Uses Bosch's reference compensation formulas (UM document) in fixed-point
 * to avoid pulling in soft-float libraries; final output converted to float.
 */
#include <stddef.h>
#include "soc_osal.h"
#include "bmp280.h"

/* Register map */
#define REG_CALIB_START   0x88
#define REG_ID            0xD0
#define REG_RESET         0xE0
#define REG_STATUS        0xF3
#define REG_CTRL_MEAS     0xF4
#define REG_CONFIG        0xF5
#define REG_PRESS_MSB     0xF7
#define REG_TEMP_MSB      0xFA

#define BMP280_CHIP_ID    0x58
#define RESET_MAGIC       0xB6

static int bmp280_read_reg(const i2c_bb_t *bus, uint8_t reg, uint8_t *buf, uint32_t n)
{
    return i2c_bb_write_read(bus, BMP280_I2C_ADDR, &reg, 1, buf, n);
}

static int bmp280_write_reg(const i2c_bb_t *bus, uint8_t reg, uint8_t val)
{
    uint8_t pkt[2] = {reg, val};
    return i2c_bb_write(bus, BMP280_I2C_ADDR, pkt, 2);
}

int bmp280_init(const i2c_bb_t *bus, bmp280_calib_t *cal)
{
    uint8_t id = 0;
    if (bmp280_read_reg(bus, REG_ID, &id, 1) != 0) return -1;
    if (id != BMP280_CHIP_ID) return -2;   /* not a BMP280 */

    /* Soft reset and wait for completion */
    bmp280_write_reg(bus, REG_RESET, RESET_MAGIC);
    osal_msleep(20);

    /* Read 24 calibration bytes (little-endian per Bosch datasheet). */
    uint8_t buf[24];
    if (bmp280_read_reg(bus, REG_CALIB_START, buf, 24) != 0) return -3;

    cal->dig_T1 = (uint16_t)(buf[0]  | (buf[1]  << 8));
    cal->dig_T2 = (int16_t) (buf[2]  | (buf[3]  << 8));
    cal->dig_T3 = (int16_t) (buf[4]  | (buf[5]  << 8));
    cal->dig_P1 = (uint16_t)(buf[6]  | (buf[7]  << 8));
    cal->dig_P2 = (int16_t) (buf[8]  | (buf[9]  << 8));
    cal->dig_P3 = (int16_t) (buf[10] | (buf[11] << 8));
    cal->dig_P4 = (int16_t) (buf[12] | (buf[13] << 8));
    cal->dig_P5 = (int16_t) (buf[14] | (buf[15] << 8));
    cal->dig_P6 = (int16_t) (buf[16] | (buf[17] << 8));
    cal->dig_P7 = (int16_t) (buf[18] | (buf[19] << 8));
    cal->dig_P8 = (int16_t) (buf[20] | (buf[21] << 8));
    cal->dig_P9 = (int16_t) (buf[22] | (buf[23] << 8));

    /* Normal mode, osrs_t=x2 (010), osrs_p=x16 (101), pmode=11 → 0b010_101_11 = 0x57 */
    if (bmp280_write_reg(bus, REG_CTRL_MEAS, 0x57) != 0) return -4;
    /* Standby 500 ms (011), IIR filter x16 (100), spi3=0 → 0b011_100_00 = 0x70 */
    if (bmp280_write_reg(bus, REG_CONFIG,    0x70) != 0) return -4;

    osal_msleep(50);  /* first sample latency */
    return 0;
}

int bmp280_read(const i2c_bb_t *bus, const bmp280_calib_t *cal,
                float *temp_c, float *pressure_hpa)
{
    if (temp_c == NULL || pressure_hpa == NULL) return -2;

    uint8_t raw[6];
    if (bmp280_read_reg(bus, REG_PRESS_MSB, raw, 6) != 0) return -1;
    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);

    /* Bosch reference compensation (32-bit fixed point) */
    int32_t var1, var2, t_fine, T;
    var1 = ((((adc_T >> 3) - ((int32_t)cal->dig_T1 << 1))) *
            ((int32_t)cal->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)cal->dig_T1)) *
              ((adc_T >> 4) - ((int32_t)cal->dig_T1))) >> 12) *
            ((int32_t)cal->dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;     /* T in 0.01 °C */

    int64_t var1_p, var2_p, p;
    var1_p = ((int64_t)t_fine) - 128000;
    var2_p = var1_p * var1_p * (int64_t)cal->dig_P6;
    var2_p = var2_p + ((var1_p * (int64_t)cal->dig_P5) << 17);
    var2_p = var2_p + (((int64_t)cal->dig_P4) << 35);
    var1_p = ((var1_p * var1_p * (int64_t)cal->dig_P3) >> 8) +
             ((var1_p * (int64_t)cal->dig_P2) << 12);
    var1_p = (((((int64_t)1) << 47) + var1_p)) * ((int64_t)cal->dig_P1) >> 33;
    if (var1_p == 0) return -3;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2_p) * 3125) / var1_p;
    var1_p = (((int64_t)cal->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2_p = (((int64_t)cal->dig_P8) * p) >> 19;
    p = ((p + var1_p + var2_p) >> 8) + (((int64_t)cal->dig_P7) << 4);
    /* p is in Q24.8, Pa */

    *temp_c       = (float)T / 100.0f;
    *pressure_hpa = (float)p / 25600.0f;   /* 256 (Q24.8) × 100 Pa/hPa */
    return 0;
}
