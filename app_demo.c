/*
 * 4T_HRM_QS2 (Hi3863 / WS63) temperature/humidity/pressure transmitter
 *
 * Sensors:
 *   SHT30   (I2C 0x44) on bit-bang bus #1 — SDA=GPIO_11, SCL=GPIO_12
 *   BMP280  (I2C 0x76) on the SAME bit-bang bus #1
 *   TM1640  (proprietary 2-wire)         — DIN=GPIO_13, CLK=GPIO_3
 *
 * Display:
 *   LCD shows full readings + status
 *   TM1640 alternates between "tt.t C  " and "hh.h H  " every 2 seconds
 */
#include <stdio.h>
#include <string.h>
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"
#include "lcd.h"

#include "sensors/i2c_bb.h"
#include "sensors/sht30.h"
#include "sensors/bmp280.h"
#include "sensors/tm1640.h"

/* ---- wiring ---- */
#define SENSOR_BUS_SDA   11
#define SENSOR_BUS_SCL   12
#define TM1640_DATA_PIN  13
#define TM1640_CLK_PIN    3

/* ---- task config ---- */
#define LCD_TASK_STACK    0x1000
#define SENSOR_TASK_STACK 0x1400
#define LCD_TASK_PRIO     26
#define SENSOR_TASK_PRIO  22
#define LCD_REFRESH_MS    200
#define SENSOR_PERIOD_MS  1000

/* I2C bus instance for SHT30+BMP280 */
static const i2c_bb_t g_sensor_bus = {
    .sda_pin = SENSOR_BUS_SDA,
    .scl_pin = SENSOR_BUS_SCL,
};

/* TM1640 device */
static const tm1640_t g_tm1640 = {
    .clk_pin  = TM1640_CLK_PIN,
    .data_pin = TM1640_DATA_PIN,
};

/* Shared readings (volatile so LCD task sees fresh values). */
volatile int g_state = 0;          /* 0 init, 1 ok, 10 init fail */
volatile float g_temp_c  = 0.0f;   /* SHT30 temp (preferred) */
volatile float g_humid_p = 0.0f;   /* SHT30 humidity */
volatile float g_bmp_temp_c = 0.0f;
volatile float g_press_hpa  = 0.0f;
volatile uint32_t g_sht30_ok = 0;
volatile uint32_t g_sht30_err = 0;
volatile uint32_t g_bmp_ok   = 0;
volatile uint32_t g_bmp_err  = 0;

static bmp280_calib_t g_bmp_cal;

static void *sensor_task(const char *arg)
{
    unused(arg);

    i2c_bb_init(&g_sensor_bus);
    osal_msleep(50);

    /* TM1640 brought up here too so we own the bus from one task. */
    tm1640_init(&g_tm1640);
    tm1640_set_brightness(&g_tm1640, 4);
    tm1640_show_text(&g_tm1640, "----    ");

    int bmp_rc = bmp280_init(&g_sensor_bus, &g_bmp_cal);
    osal_printk("[sensor] bmp280_init rc=%d\r\n", bmp_rc);
    if (bmp_rc != 0) {
        /* Don't bail out — SHT30 may still work even if BMP280 isn't wired. */
        osal_printk("[sensor] BMP280 not responding (chip id mismatch / wiring?)\r\n");
    }

    g_state = 1;
    uint32_t loop = 0;
    char dispbuf[16];

    for (;;) {
        float t = 0, h = 0;
        if (sht30_read(&g_sensor_bus, &t, &h) == 0) {
            g_temp_c  = t;
            g_humid_p = h;
            g_sht30_ok++;
        } else {
            g_sht30_err++;
        }

        float bt = 0, bp = 0;
        if (bmp280_read(&g_sensor_bus, &g_bmp_cal, &bt, &bp) == 0) {
            g_bmp_temp_c = bt;
            g_press_hpa  = bp;
            g_bmp_ok++;
        } else {
            g_bmp_err++;
        }

        /* Alternate display every 2 seconds between temp and humidity. */
        if ((loop / 2) & 1) {
            /* humidity, e.g. "65.4 H " (6 digits used) */
            int whole = (int)g_humid_p;
            int frac  = (int)((g_humid_p - whole) * 10);
            if (frac < 0) frac = 0;
            snprintf(dispbuf, sizeof(dispbuf), "%2d.%dH", whole, frac);
        } else {
            /* temperature, e.g. "25.6 C " */
            float tt = g_temp_c;
            int whole = (int)tt;
            int frac  = (int)((tt - whole) * 10);
            if (frac < 0) frac = -frac;
            snprintf(dispbuf, sizeof(dispbuf), "%2d.%dC", whole, frac);
        }
        tm1640_show_text(&g_tm1640, dispbuf);

        loop++;
        osal_printk("[sensor] T=%d.%d C  RH=%d.%d %%  P=%d.%d hPa  (sht30_ok=%lu err=%lu, bmp_ok=%lu err=%lu)\r\n",
                    (int)g_temp_c, (int)((g_temp_c - (int)g_temp_c) * 10),
                    (int)g_humid_p, (int)((g_humid_p - (int)g_humid_p) * 10),
                    (int)g_press_hpa, (int)((g_press_hpa - (int)g_press_hpa) * 10),
                    (unsigned long)g_sht30_ok, (unsigned long)g_sht30_err,
                    (unsigned long)g_bmp_ok,   (unsigned long)g_bmp_err);
        osal_msleep(SENSOR_PERIOD_MS);
    }
    return NULL;
}

static void *lcd_task(const char *arg)
{
    unused(arg);
    char hdr[]  = "WS63 T/H/P sensor";
    char pin1[] = "Bus1 SDA11/SCL12";
    char pin2[] = "Tube DAT13/CLK3";
    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)hdr);
    spi_lcd_display_string_line(0, 1, WHITE, BLACK, (uint8_t *)pin1);
    spi_lcd_display_string_line(0, 2, WHITE, BLACK, (uint8_t *)pin2);

    char tline[32];
    char hline[32];
    char pline[32];
    char stat[40];
    uint32_t tick = 0;
    for (;;) {
        snprintf(tline, sizeof(tline), "Temp:    %2d.%d C   ",
                 (int)g_temp_c, (int)((g_temp_c - (int)g_temp_c) * 10));
        snprintf(hline, sizeof(hline), "Humid:   %2d.%d %%   ",
                 (int)g_humid_p, (int)((g_humid_p - (int)g_humid_p) * 10));
        snprintf(pline, sizeof(pline), "Press: %4d.%d hPa ",
                 (int)g_press_hpa, (int)((g_press_hpa - (int)g_press_hpa) * 10));
        spi_lcd_display_string_line(0, 4, GREEN, BLACK, (uint8_t *)tline);
        spi_lcd_display_string_line(0, 5, GREEN, BLACK, (uint8_t *)hline);
        spi_lcd_display_string_line(0, 6, WHITE, BLACK, (uint8_t *)pline);

        snprintf(stat, sizeof(stat), "ok S:%lu B:%lu err S:%lu B:%lu",
                 (unsigned long)g_sht30_ok, (unsigned long)g_bmp_ok,
                 (unsigned long)g_sht30_err, (unsigned long)g_bmp_err);
        spi_lcd_display_string_line(0, 8, WHITE, BLACK, (uint8_t *)stat);

        char counter[24];
        tick++;
        snprintf(counter, sizeof(counter), "Uptime: %lu s",
                 (unsigned long)(tick * LCD_REFRESH_MS / 1000));
        spi_lcd_display_string_line(0, 9, WHITE, BLACK, (uint8_t *)counter);
        osal_msleep(LCD_REFRESH_MS);
    }
    return NULL;
}

static void app_entry(void)
{
    osal_task *h;
    osal_kthread_lock();
    h = osal_kthread_create((osal_kthread_handler)lcd_task, NULL,
                            "LcdTask", LCD_TASK_STACK);
    if (h) osal_kthread_set_priority(h, LCD_TASK_PRIO);

    h = osal_kthread_create((osal_kthread_handler)sensor_task, NULL,
                            "SensorTask", SENSOR_TASK_STACK);
    if (h) osal_kthread_set_priority(h, SENSOR_TASK_PRIO);
    osal_kthread_unlock();
}

app_run(app_entry);
