/*
 * 4T_HRM_QS2 (Hi3863 / WS63) combined demo
 *
 *   LCD       : "Hello WS63" at top, shows last button press, uptime counter
 *   3 LEDs    : red/green/yellow blink together every 1s
 *   Buzzer    : beep ~150ms every 10s
 *   4 buttons : ADC multiplex, each shows its own message on LCD + UART print
 *
 * Pin map (from 4T_HRM_QS2 protocol doc):
 *   GPIO_03  LCD WR/DC          GPIO_07  BUZZ
 *   GPIO_05  LCD CS             GPIO_08  ADC_KEY (4 buttons via voltage divider)
 *   GPIO_10  LED1 (red)         GPIO_11  LED2 (green)         GPIO_12  LED3 (yellow)
 *   GPIO_01/04/06  SPI MOSI/MISO/CLK (driven by lcd.c)
 */

#include <stdio.h>
#include <string.h>
#include "common_def.h"
#include "pinctrl.h"
#include "gpio.h"
#include "pwm.h"
#include "soc_osal.h"
#include "app_init.h"
#include "adc.h"
#include "adc_porting.h"
#include "osal_debug.h"
#include "lcd.h"

/* ---- pin / channel ---- */
#define LED_RED_PIN        10
#define LED_GREEN_PIN      11
#define LED_YELLOW_PIN     12
#define BUZZ_PIN           7
#define BUZZ_PWM_CHANNEL   7
#define BUZZ_PWM_GROUP     0

/* ---- timing ---- */
#define LED_HALF_PERIOD_MS    500    /* 500ms on + 500ms off = 1Hz blink */
#define BUZZ_INTERVAL_MS      10000  /* beep every 10s */
#define BUZZ_DURATION_MS      150
#define ADC_POLL_ENABLE_MS    10
#define ADC_POLL_REST_MS      5
#define LCD_REFRESH_MS        100

/* ---- task config ---- */
#define TASK_STACK_SIZE       0x1000
#define LCD_TASK_PRIO         26
#define LED_TASK_PRIO         24
#define BUZZ_TASK_PRIO        23
#define ADC_TASK_PRIO         20

/* ---- ADC button debounce (same logic as board's adc_test.c) ---- */
#define NUM_KEYS              4
#define DEBOUNCE_MS           2

static uint8_t  key_pressed[NUM_KEYS]       = {0};
static uint64_t press_start[NUM_KEYS]       = {0};
static uint64_t release_start[NUM_KEYS]     = {0};
static uint8_t  press_debouncing[NUM_KEYS]  = {0};
static uint8_t  release_debouncing[NUM_KEYS] = {0};

/* shared event state: LCD task polls g_key_seq for changes */
static volatile int      g_last_key = -1;
static volatile uint32_t g_key_seq  = 0;

/* =========================================================
 *  LED
 * ========================================================= */
static void led_init(void)
{
    int pins[] = {LED_RED_PIN, LED_GREEN_PIN, LED_YELLOW_PIN};
    for (int i = 0; i < 3; i++) {
        uapi_pin_set_mode(pins[i], HAL_PIO_FUNC_GPIO);
        uapi_gpio_set_dir(pins[i], GPIO_DIRECTION_OUTPUT);
        uapi_gpio_set_val(pins[i], GPIO_LEVEL_LOW);
    }
}

static void *led_task(const char *arg)
{
    unused(arg);
    led_init();
    int pins[] = {LED_RED_PIN, LED_GREEN_PIN, LED_YELLOW_PIN};
    int on = 0;
    for (;;) {
        on = !on;
        int level = on ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
        for (int i = 0; i < 3; i++) {
            uapi_gpio_set_val(pins[i], level);
        }
        osal_msleep(LED_HALF_PERIOD_MS);
    }
    return NULL;
}

/* =========================================================
 *  Buzzer (PWM channel 7 on GPIO_07)
 * ========================================================= */
static void *buzz_task(const char *arg)
{
    unused(arg);
    /* 1kHz square wave, 50% duty (matches board's pwm_t.c) */
    pwm_config_t cfg = {10000, 10000, 0, 0xFF, true};
    uint8_t channel_id = BUZZ_PWM_CHANNEL;

    uapi_pin_set_mode(BUZZ_PIN, 1);   /* PWM function */
    uapi_pwm_deinit();
    uapi_pwm_init();
    uapi_pwm_open(BUZZ_PWM_CHANNEL, &cfg);
    /* WS63 PWM is V151 — group-based start/stop */
    uapi_pwm_set_group(BUZZ_PWM_GROUP, &channel_id, 1);

    for (;;) {
        osal_msleep(BUZZ_INTERVAL_MS);
        uapi_pwm_start_group(BUZZ_PWM_GROUP);
        osal_msleep(BUZZ_DURATION_MS);
        uapi_pwm_stop_group(BUZZ_PWM_GROUP);
    }
    return NULL;
}

/* =========================================================
 *  ADC buttons (4 keys multiplexed on ADC ch1 / GPIO_08)
 *  Voltage thresholds copied verbatim from board's adc_test.c
 * ========================================================= */
static void adc_callback(uint8_t ch, uint32_t *buffer, uint32_t length, bool *next)
{
    UNUSED(ch);
    UNUSED(next);
    uint64_t now = osal_get_jiffies();

    for (uint32_t i = 0; i < length; i++) {
        uint32_t voltage = buffer[i];
        int key_id = -1;
        if      (voltage <= 500)                       key_id = 0;
        else if (voltage >= 1000 && voltage <= 2000)   key_id = 1;
        else if (voltage >= 2100 && voltage <= 2500)   key_id = 2;
        else if (voltage >= 2600 && voltage <= 2800)   key_id = 3;

        if (key_id != -1) {
            if (key_pressed[key_id] == 0 && !press_debouncing[key_id]) {
                press_debouncing[key_id] = 1;
                press_start[key_id]      = now;
            } else if (press_debouncing[key_id]) {
                if (now - press_start[key_id] >= DEBOUNCE_MS) {
                    key_pressed[key_id]      = 1;
                    press_debouncing[key_id] = 0;
                }
            }
            if (key_pressed[key_id] == 1) {
                release_debouncing[key_id] = 0;
            }
        } else {
            for (int k = 0; k < NUM_KEYS; k++) {
                if (key_pressed[k] == 1 && !release_debouncing[k]) {
                    release_debouncing[k] = 1;
                    release_start[k]      = now;
                } else if (release_debouncing[k]) {
                    if (now - release_start[k] >= DEBOUNCE_MS) {
                        g_last_key = k;
                        g_key_seq++;
                        osal_printk("KEY%d pressed\r\n", k + 1);
                        key_pressed[k]        = 0;
                        release_debouncing[k] = 0;
                    }
                }
            }
        }
    }
}

static void *adc_task(const char *arg)
{
    UNUSED(arg);
    uapi_adc_init(ADC_CLOCK_500KHZ);
    uapi_adc_power_en(AFE_SCAN_MODE_MAX_NUM, true);
    adc_scan_config_t config = { .type = 0, .freq = 1 };

    for (;;) {
        uapi_adc_auto_scan_ch_enable(1, config, adc_callback);
        osal_msleep(ADC_POLL_ENABLE_MS);
        uapi_adc_auto_scan_ch_disable(1);
        osal_msleep(ADC_POLL_REST_MS);
    }
    return NULL;
}

/* =========================================================
 *  LCD: header + key feedback + uptime counter
 * ========================================================= */
static const char *key_messages[NUM_KEYS] = {
    "KEY1: Hello!     ",
    "KEY2: Pressed!   ",
    "KEY3: WS63 demo  ",
    "KEY4: Bye!       ",
};

static void *lcd_task(const char *arg)
{
    unused(arg);
    char hello[]  = "Hello WS63";
    char hint[]   = "Press KEY 1-4:";
    char blank[]  = "                   ";
    char counter[32];

    spi_lcd_init();
    spi_lcd_clear(BLACK);
    spi_lcd_display_string_line(0, 0, WHITE, BLACK, (uint8_t *)hello);
    spi_lcd_display_string_line(0, 2, WHITE, BLACK, (uint8_t *)hint);
    spi_lcd_display_string_line(0, 3, WHITE, BLACK, (uint8_t *)blank);

    uint32_t last_seen_seq = 0;
    uint32_t tick          = 0;

    for (;;) {
        if (g_key_seq != last_seen_seq) {
            last_seen_seq = g_key_seq;
            int k = g_last_key;
            if (k >= 0 && k < NUM_KEYS) {
                spi_lcd_display_string_line(0, 3, WHITE, BLACK, (uint8_t *)blank);
                spi_lcd_display_string_line(0, 3, GREEN, BLACK,
                                            (uint8_t *)key_messages[k]);
            }
        }
        tick++;
        snprintf(counter, sizeof(counter), "Uptime: %lu s",
                 (unsigned long)(tick * LCD_REFRESH_MS / 1000));
        spi_lcd_display_string_line(0, 5, WHITE, BLACK, (uint8_t *)counter);
        osal_msleep(LCD_REFRESH_MS);
    }
    return NULL;
}

/* =========================================================
 *  Entry — register all tasks
 * ========================================================= */
static void app_entry(void)
{
    osal_task *h;
    osal_kthread_lock();

    h = osal_kthread_create((osal_kthread_handler)lcd_task,  NULL, "LcdTask",  TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, LCD_TASK_PRIO);

    h = osal_kthread_create((osal_kthread_handler)led_task,  NULL, "LedTask",  TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, LED_TASK_PRIO);

    h = osal_kthread_create((osal_kthread_handler)buzz_task, NULL, "BuzzTask", TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, BUZZ_TASK_PRIO);

    h = osal_kthread_create((osal_kthread_handler)adc_task,  NULL, "AdcTask",  TASK_STACK_SIZE);
    if (h) osal_kthread_set_priority(h, ADC_TASK_PRIO);

    osal_kthread_unlock();
}

app_run(app_entry);
