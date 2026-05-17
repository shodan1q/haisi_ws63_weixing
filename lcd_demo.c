#include "pinctrl.h"
#include "gpio.h"
#include "soc_osal.h"
#include "app_init.h"

#include "lcd.h"

#define TASK_DELAY_TIME 100

/* static void *sw_spi_test_task(const char *arg)
{
    unused(arg);
    osal_printk("\r\n***** Software LCD Flash Test Start *****\r\n");
    osal_msleep(1000);
    spi_lcd_init();
    while (1) {
        spi_lcd_clear(BLUE);
         osal_msleep(500);
        spi_lcd_clear(WHITE);
         osal_msleep(500);
        spi_lcd_clear(RED);
         osal_msleep(500);

    }
    return NULL;
} */
static void *sw_spi_test_task(const char *arg)
{
    uint8_t str0[] = "Line 0";
    uint8_t str1[] = "Line 1";
    uint8_t str2[] = "Line 2";
    uint8_t str3[] = "Line 3";
    uint8_t str4[] = "Line 4";
    uint8_t str5[] = "Line 5";
    uint8_t str6[] = "Line 6";
    uint8_t str7[] = "Line 7";
    uint8_t str8[] = "Line 8";
    uint8_t str9[] = "Line 9";
    unused(arg);
    osal_printk("\r\n***** Software LCD Flash Test Start *****\r\n");
    osal_msleep(1000);
    
    // 初始化LCD
    spi_lcd_init();
    
    // 清屏白色
    spi_lcd_clear(WHITE);
    
    // 在屏幕中间显示HELLO
    // 假设屏幕分辨率320x240，字体16x24
    spi_lcd_clear(BLUE2);
    
    spi_lcd_display_string_line(0,0, BLACK, WHITE, str0);  
    spi_lcd_display_string_line(1,1, BLACK, WHITE, str1);  
    spi_lcd_display_string_line(2,2, BLACK, WHITE, str2);  
    spi_lcd_display_string_line(3,3, BLACK, WHITE, str3);  
    spi_lcd_display_string_line(4,4, BLACK, WHITE, str4);  
    spi_lcd_display_string_line(5,5, BLACK, WHITE, str5);  
    spi_lcd_display_string_line(6,6, BLACK, WHITE, str6);  
    spi_lcd_display_string_line(7,7, BLACK, WHITE, str7);  
    spi_lcd_display_string_line(8,8, BLACK, WHITE, str8);  
    spi_lcd_display_string_line(9,9, BLACK, WHITE, str9);  
    
    
    
    while (1) 
    {
        
        
    }
    
    return NULL;
}
static void base_lcd_demo(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)sw_spi_test_task, 0, "SwSpiTestTask", 0x1000);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, 24);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}
app_run(base_lcd_demo);

