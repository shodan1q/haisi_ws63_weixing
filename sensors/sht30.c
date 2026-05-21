#include "soc_osal.h"
#include "sht30.h"

/* Single-shot, high repeatability, clock stretching disabled (0x24 0x00). */
static const uint8_t SHT30_CMD_MEASURE[2] = {0x24, 0x00};

/* CRC-8 with polynomial 0x31, init 0xFF — per Sensirion datasheet. */
static uint8_t sht30_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

int sht30_read(const i2c_bb_t *bus, float *temp_c, float *humid_p)
{
    if (temp_c == NULL || humid_p == NULL) return -2;

    /* Trigger measurement */
    if (i2c_bb_write(bus, SHT30_I2C_ADDR, SHT30_CMD_MEASURE, 2) != 0) return -1;

    /* SHT30 high-repeatability takes ~15 ms */
    osal_msleep(20);

    /* Read 6 bytes: T_MSB T_LSB T_CRC H_MSB H_LSB H_CRC */
    uint8_t buf[6] = {0};
    if (i2c_bb_read(bus, SHT30_I2C_ADDR, buf, 6) != 0) return -1;

    if (sht30_crc8(buf, 2) != buf[2]) return 1;
    if (sht30_crc8(buf + 3, 2) != buf[5]) return 1;

    uint16_t t_raw = (uint16_t)((buf[0] << 8) | buf[1]);
    uint16_t h_raw = (uint16_t)((buf[3] << 8) | buf[4]);

    *temp_c  = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    *humid_p = 100.0f * ((float)h_raw / 65535.0f);
    return 0;
}
