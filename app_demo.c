/*
 * 4T_HRM_QS2 (Hi3863 / WS63) T/H/P sensor transmitter
 *
 * Sensors:
 *   SHT30   (I2C 0x44) on bit-bang bus  — SDA=GPIO_7,  SCL=GPIO_8
 *   BMP280  (I2C 0x76) on the same bus  — same SDA/SCL
 *   TM1640  (2-wire serial, NOT i2c)    — DIN=GPIO_13, CLK=GPIO_14
 *
 * GPIO_14 was originally FLASH_CS on the board; we re-use it because the
 * on-board flash chip isn't part of this demo. The LCD's WR/DC line on
 * GPIO_3 is now free, so the on-board LCD task is re-enabled.
 */
#include <stdio.h>
#include <string.h>
#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"

#define ENABLE_LCD 1     /* GPIO_3 is back to LCD D/C; tube CLK moved to GPIO_14 */

#if ENABLE_LCD
#include "lcd.h"
#endif

#include "sensors/i2c_bb.h"
#include "sensors/sht30.h"
#include "sensors/bmp280.h"
#include "sensors/tm1640.h"

/* ---- wiring ---- */
#define SENSOR_BUS_SDA    7
#define SENSOR_BUS_SCL    8
#define TM1640_DATA_PIN  13
#define TM1640_CLK_PIN   14

/* ---- task config ---- */
#define SENSOR_TASK_STACK 0x1400
#define LCD_TASK_STACK    0x1000
#define SENSOR_TASK_PRIO  22
#define LCD_TASK_PRIO     26
#define SENSOR_PERIOD_MS  1000

static const i2c_bb_t g_sensor_bus = { .sda_pin = SENSOR_BUS_SDA, .scl_pin = SENSOR_BUS_SCL };
static const tm1640_t g_tm1640     = { .clk_pin = TM1640_CLK_PIN, .data_pin = TM1640_DATA_PIN };
static bmp280_calib_t g_bmp_cal;

volatile float    g_temp_c     = 0.0f;
volatile float    g_humid_p    = 0.0f;
volatile float    g_press_hpa  = 0.0f;
volatile uint32_t g_sht30_ok   = 0;
volatile uint32_t g_sht30_err  = 0;
volatile uint32_t g_bmp_ok     = 0;
volatile uint32_t g_bmp_err    = 0;

static void *sensor_task(const char *arg)
{
    unused(arg);

    osal_printk("\r\n========== [boot] sensor_task started ==========\r\n");
    osal_printk("[boot] SHT30+BMP280 on SDA=GPIO_%d SCL=GPIO_%d\r\n",
                SENSOR_BUS_SDA, SENSOR_BUS_SCL);
    osal_printk("[boot] TM1640 on DIN=GPIO_%d CLK=GPIO_%d\r\n",
                TM1640_DATA_PIN, TM1640_CLK_PIN);

    /* --- Step 1: TM1640 first, then a visible test pattern ----------- */
    osal_printk("[boot] tm1640_init...\r\n");
    tm1640_init(&g_tm1640);

    osal_printk("[boot] tm1640 set brightness max...\r\n");
    tm1640_set_brightness(&g_tm1640, 8);

    osal_printk("[boot] tm1640 all-segments-on test...\r\n");
    /* 16 bytes of 0xFF = every segment (incl. dp) lit on every digit */
    uint8_t allon[16];
    memset(allon, 0xFF, sizeof(allon));
    tm1640_write(&g_tm1640, allon, sizeof(allon), 0);
    osal_msleep(2000);          /* keep visible for 2 s — confirm tube wiring works */

    osal_printk("[boot] tm1640 -> 'INIT'\r\n");
    tm1640_show_text(&g_tm1640, "INIT    ");
    osal_msleep(500);

    /* --- Step 2: I2C bus & sensors ----------------------------------- */
    osal_printk("[boot] i2c_bb_init bus...\r\n");
    i2c_bb_init(&g_sensor_bus);
    osal_msleep(50);

    osal_printk("[boot] sht30: probing (try one read)...\r\n");
    {
        float t = 0, h = 0;
        int rc = sht30_read(&g_sensor_bus, &t, &h);
        osal_printk("[boot] sht30 probe rc=%d  t=%d h=%d\r\n", rc, (int)t, (int)h);
    }

    osal_printk("[boot] bmp280_init...\r\n");
    int brc = bmp280_init(&g_sensor_bus, &g_bmp_cal);
    osal_printk("[boot] bmp280_init rc=%d (0=OK, -1=I2C fail, -2=wrong chip id, -3=calib fail)\r\n", brc);

    osal_printk("[boot] sensor_task entering main loop\r\n");

    uint32_t loop = 0;
    char dispbuf[16];
    for (;;) {
        /* SHT30 */
        float t = 0, h = 0;
        int sht_rc = sht30_read(&g_sensor_bus, &t, &h);
        if (sht_rc == 0) {
            g_temp_c  = t;
            g_humid_p = h;
            g_sht30_ok++;
        } else {
            g_sht30_err++;
        }

        /* BMP280 */
        float bt = 0, bp = 0;
        int bmp_rc = bmp280_read(&g_sensor_bus, &g_bmp_cal, &bt, &bp);
        if (bmp_rc == 0) {
            g_press_hpa = bp;
            g_bmp_ok++;
        } else {
            g_bmp_err++;
        }

        /* TM1640 — ESP32-style rotating display (like test_tm1640's cycle
         * through `segments` / `segments2` / `number`). We rotate between
         * two screens, holding each for SCREEN_HOLD_S seconds:
         *
         *   page 0  "25.6 65.3"  → 7 digits: T int, T units+dp, T frac,
         *                          blank, H int, H units+dp, H frac
         *   page 1  "P 1013.2"   → 7 digits: 'P', blank, 4 pressure
         *                          digits with dp before last
         */
        #define SCREEN_HOLD_S  3
        int page = (loop / SCREEN_HOLD_S) % 2;
        if (page == 0) {
            int t_int  = (int)g_temp_c;
            int t_frac = (int)((g_temp_c - t_int) * 10);
            if (t_frac < 0) t_frac = -t_frac;
            int h_int  = (int)g_humid_p;
            int h_frac = (int)((g_humid_p - h_int) * 10);
            if (h_frac < 0) h_frac = 0;
            snprintf(dispbuf, sizeof(dispbuf), "%2d.%d %2d.%d",
                     t_int, t_frac, h_int, h_frac);
        } else {
            int p_int  = (int)g_press_hpa;
            int p_frac = (int)((g_press_hpa - p_int) * 10);
            if (p_frac < 0) p_frac = 0;
            /* "P %4d.%d" → e.g. "P 1013.2" → 8 chars; after dp merge that
             * leaves exactly 7 digit positions: 'P', ' ', '1','0','1',
             * '3'+dp, '2' */
            snprintf(dispbuf, sizeof(dispbuf), "P %4d.%d", p_int, p_frac);
        }
        tm1640_show_text(&g_tm1640, dispbuf);

        osal_printk("[loop %lu] sht30 rc=%d T=%d.%d H=%d.%d  "
                    "bmp rc=%d P=%d.%d  ok=S%lu/B%lu err=S%lu/B%lu  show=\"%s\"\r\n",
                    (unsigned long)loop, sht_rc,
                    (int)t, (int)((t - (int)t) * 10),
                    (int)h, (int)((h - (int)h) * 10),
                    bmp_rc, (int)bp, (int)((bp - (int)bp) * 10),
                    (unsigned long)g_sht30_ok, (unsigned long)g_bmp_ok,
                    (unsigned long)g_sht30_err, (unsigned long)g_bmp_err,
                    dispbuf);

        loop++;
        osal_msleep(SENSOR_PERIOD_MS);
    }
    return NULL;
}

#if ENABLE_LCD
static void *lcd_task(const char *arg)
{
    unused(arg);
    char hdr[]  = "WS63 T/H/P sensor";
    char pins[] = "SDA7/SCL8 Tube14/13";
    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, GREEN, BLACK, (uint8_t *)hdr);
    spi_lcd_display_string_line(0, 1, WHITE, BLACK, (uint8_t *)pins);

    char tline[40], hline[40], pline[40], stat[40];
    for (;;) {
        snprintf(tline, sizeof(tline), "Temp:    %2d.%d C   ",
                 (int)g_temp_c, (int)((g_temp_c - (int)g_temp_c) * 10));
        snprintf(hline, sizeof(hline), "Humid:   %2d.%d %%   ",
                 (int)g_humid_p, (int)((g_humid_p - (int)g_humid_p) * 10));
        snprintf(pline, sizeof(pline), "Press: %4d.%d hPa ",
                 (int)g_press_hpa, (int)((g_press_hpa - (int)g_press_hpa) * 10));
        snprintf(stat, sizeof(stat), "ok S:%lu B:%lu err:%lu/%lu",
                 (unsigned long)g_sht30_ok, (unsigned long)g_bmp_ok,
                 (unsigned long)g_sht30_err, (unsigned long)g_bmp_err);
        spi_lcd_display_string_line(0, 3, GREEN, BLACK, (uint8_t *)tline);
        spi_lcd_display_string_line(0, 4, GREEN, BLACK, (uint8_t *)hline);
        spi_lcd_display_string_line(0, 5, WHITE, BLACK, (uint8_t *)pline);
        spi_lcd_display_string_line(0, 7, WHITE, BLACK, (uint8_t *)stat);
        osal_msleep(200);
    }
    return NULL;
}
#endif /* ENABLE_LCD */

static void app_entry(void)
{
    osal_printk("\r\n[app_entry] starting sensor demo\r\n");
    osal_task *h;
    osal_kthread_lock();
#if ENABLE_LCD
    h = osal_kthread_create((osal_kthread_handler)lcd_task, NULL,
                            "LcdTask", LCD_TASK_STACK);
    if (h) osal_kthread_set_priority(h, LCD_TASK_PRIO);
#endif
    h = osal_kthread_create((osal_kthread_handler)sensor_task, NULL,
                            "SensorTask", SENSOR_TASK_STACK);
    if (h) osal_kthread_set_priority(h, SENSOR_TASK_PRIO);
    osal_kthread_unlock();
}

app_run(app_entry);
