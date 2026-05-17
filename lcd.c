/*
 * Copyright (c) 2024 Beijing HuaQingYuanJian Education Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 ******************************************************************************
 * @file   bsp_ili9341_4line.c
 * @brief  2.8寸屏ili9341驱动文件，采用4线SPI
 *
 ******************************************************************************
 */
#include "lcd.h"
#include "fonts.h"

#define SW_SPI_SCK_PIN   6   // 时钟线，接 Flash CLK
#define SW_SPI_MOSI_PIN  1   // 主设备输出，接 Flash DI (或称为MOSI)
#define SW_SPI_MISO_PIN  4   // 主设备输入，接 Flash DO (或称为MISO)
#define SW_FLASH_CS_PIN  14   // 片选线，接 Flash CS
#define SW_LCD_CS_PIN    5  // 片选线，接 LCD CS
#define SW_SD_CS_PIN     2  // 片选线，接 SD CS
#define SW_RFID_CS_PIN   13   // 片选线，接 RFID CS
#define SW_LCD_WR_PIN    3  // 片选线，接 RFID CS


#define SPI_DC_0   uapi_gpio_set_val(SW_LCD_WR_PIN, GPIO_LEVEL_LOW);
#define SPI_DC_1   uapi_gpio_set_val(SW_LCD_WR_PIN, GPIO_LEVEL_HIGH);
#define LCD_CS_0   uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_LOW);
#define LCD_CS_1   uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_HIGH);

static void sw_spi_init_pins(void)
{
    /* 将所用引脚设置为GPIO模式 */
    uapi_pin_set_mode(SW_FLASH_CS_PIN, HAL_PIO_FUNC_GPIO);      // CS 输出
    uapi_pin_set_mode(SW_LCD_CS_PIN, PIN_MODE_4);      // CS 输出
    uapi_pin_set_mode(SW_SD_CS_PIN, HAL_PIO_FUNC_GPIO);      // CS 输出
    uapi_pin_set_mode(SW_RFID_CS_PIN, HAL_PIO_FUNC_GPIO);      // CS 输出

    uapi_pin_set_mode(SW_SPI_SCK_PIN,  HAL_PIO_FUNC_GPIO);      // SCK 输出
    uapi_pin_set_mode(SW_SPI_MOSI_PIN, HAL_PIO_FUNC_GPIO);      // MOSI 输出
    uapi_pin_set_mode(SW_SPI_MISO_PIN, PIN_MODE_2);             // MISO 输入

    uapi_pin_set_mode(SW_LCD_WR_PIN, HAL_PIO_FUNC_GPIO);      // MOSI 输出

    uapi_gpio_set_dir(SW_FLASH_CS_PIN, GPIO_DIRECTION_OUTPUT);  // FLASH 输出
    uapi_gpio_set_dir(SW_LCD_CS_PIN, GPIO_DIRECTION_OUTPUT);    // LCD 输出
    uapi_gpio_set_dir(SW_SD_CS_PIN, GPIO_DIRECTION_OUTPUT);     // SD 输出
    uapi_gpio_set_dir(SW_RFID_CS_PIN, GPIO_DIRECTION_OUTPUT);   // RFID 输出

    uapi_gpio_set_dir(SW_SPI_SCK_PIN, GPIO_DIRECTION_OUTPUT);   // SCK 输出
    uapi_gpio_set_dir(SW_SPI_MOSI_PIN, GPIO_DIRECTION_OUTPUT);  // MOSI 输出
    uapi_gpio_set_dir(SW_SPI_MISO_PIN, GPIO_DIRECTION_INPUT);   // MISO 输入（注意是INPUT！）

    uapi_gpio_set_dir(SW_LCD_WR_PIN, GPIO_DIRECTION_OUTPUT);  // MOSI 输出
    
    /* 初始状态: CS高(不选中), SCK低(模式0空闲状态), MOSI低 */
    uapi_gpio_set_val(SW_FLASH_CS_PIN, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(SW_SD_CS_PIN, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(SW_RFID_CS_PIN, GPIO_LEVEL_HIGH);

    uapi_gpio_set_val(SW_SPI_SCK_PIN, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(SW_SPI_MOSI_PIN, GPIO_LEVEL_LOW);
    uapi_gpio_set_val(SW_LCD_WR_PIN, GPIO_LEVEL_HIGH);
}

/* static void sw_spi_delay_us(uint32_t us)
{
    osal_udelay(us);
} */

static uint8_t sw_spi_read_pin(uint8_t pin)
{
    // 注意：需要确认您的SDK中读取电平的函数名
    // 如果 uapi_gpio_get_val 存在且返回 GPIO_LEVEL_HIGH/LOW，则直接使用
    // 否则可能需要其他方式读取
    // 这里假设存在 uapi_gpio_get_val 函数
    return (uapi_gpio_get_val(pin) == GPIO_LEVEL_HIGH) ? 1 : 0;
}

static uint8_t sw_spi_transfer_byte(uint8_t tx_byte)
{
    uint8_t rx_byte = 0;
    
    /* 模式0：时钟空闲低电平，数据在上升沿采样 */
    for(int8_t i = 7; i >= 0; i--) { // 高位(MSB)先传
        
        /* 1. 准备要发送的位 (在时钟上升沿之前稳定) */
        if(tx_byte & (1 << i)) {
            uapi_gpio_set_val(SW_SPI_MOSI_PIN, GPIO_LEVEL_HIGH);
        } else {
            uapi_gpio_set_val(SW_SPI_MOSI_PIN, GPIO_LEVEL_LOW);
        }
        //sw_spi_delay_us(1); // 短暂延时确保数据稳定
        
        /* 2. 产生时钟上升沿，从机在此刻采样MOSI数据 */
        uapi_gpio_set_val(SW_SPI_SCK_PIN, GPIO_LEVEL_HIGH);
        
        /* 3. 主机在时钟高电平期间采样MISO数据 */
        if(sw_spi_read_pin(SW_SPI_MISO_PIN)) {
            rx_byte |= (1 << i);
        }
        //sw_spi_delay_us(1);
        
        /* 4. 产生时钟下降沿，为下一次数据更新做准备 */
        uapi_gpio_set_val(SW_SPI_SCK_PIN, GPIO_LEVEL_LOW);
        //sw_spi_delay_us(1);
    }
    
    return rx_byte;
}

void lcd_send_cmd(uint8_t tdata)
{
    uapi_gpio_set_val(SW_LCD_WR_PIN, GPIO_LEVEL_LOW);

    uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_LOW);
    //sw_spi_delay_us(1); // 等待Flash准备

    sw_spi_transfer_byte(tdata);
        
    uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_HIGH);
    //sw_spi_delay_us(1); // 等待Flash准备
}

/**
 * @brief 批量发送数据到LCD（软件SPI版本）
 * @param TxData 要发送的数据缓冲区
 * @param size 数据字节数
 */
void TFT_SEND_DATA_NOT_SINGLE(uint8_t *TxData, uint16_t size)
{
    /* 设置DC引脚为数据模式 */
    uapi_gpio_set_val(SW_LCD_WR_PIN, GPIO_LEVEL_HIGH);
    
    /* 选中LCD（片选拉低） */
    uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_LOW);
    
    /* 批量发送数据 */
    for(uint16_t i = 0; i < size; i++) {
        sw_spi_transfer_byte(TxData[i]);
    }
    
    /* 取消选中LCD（片选拉高） */
    uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_HIGH);
}

void lcd_send_data(uint8_t tdata)
{
    uapi_gpio_set_val(SW_LCD_WR_PIN, GPIO_LEVEL_HIGH);

    uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_LOW);

    sw_spi_transfer_byte(tdata);
        
    uapi_gpio_set_val(SW_LCD_CS_PIN, GPIO_LEVEL_HIGH);
}

void spi_lcd_init(void)
{
	sw_spi_init_pins();
	/*软复位*/
	/*
	 uapi_gpio_set_val(SW_SD_CS_PIN, GPIO_LEVEL_LOW);
	 osal_msleep(200);
	 uapi_gpio_set_val(SW_SD_CS_PIN, GPIO_LEVEL_HIGH);*/
	 lcd_send_cmd(SWRESET);
	osal_msleep(500);
	/*关闭睡眠模式*/
    lcd_send_cmd(SLPOUT); 			
	osal_msleep(1000);        
	/*设置RGB格式为565*/
	lcd_send_cmd(COLMOD);        
	lcd_send_data(0x05);
	/*设置VCOM补偿电压  0x1A = -0.15V*/
	lcd_send_cmd(VCMOFSET); 		
	lcd_send_data(0x1A);
	/*设置屏幕显示方向*/
	lcd_send_cmd(0x36);               
	lcd_send_data(0x64);
	/*设置行距*/
	lcd_send_cmd(0xb2);		
	lcd_send_data(0x03);
	lcd_send_data(0x03);
	lcd_send_data(0x00);
	lcd_send_data(0x33);
	lcd_send_data(0x33);
	/*门控制  VGH 12.2v   VGL -10.43v*/
	lcd_send_cmd(0xb7);			
	lcd_send_data(0x05);			
	/*VCOM设置  1.675 */
	lcd_send_cmd(0xBB);
	lcd_send_data(0x3F);
	/*设置LCM 控制*/
	lcd_send_cmd(0xC0); 
	lcd_send_data(0x2c);
	/*VDV和VRH命令使能*/
	lcd_send_cmd(0xC2);		
	lcd_send_data(0x01);
	/*设置VRH 4.3+( vcom+vcom offset+vdv)  */
	lcd_send_cmd(0xC3);			
	lcd_send_data(0x0F);		
	/*设置VDV  0v */
	lcd_send_cmd(0xC4);			
	lcd_send_data(0x20);				
	/*正常模式下的帧速率控制 111HZ*/
	lcd_send_cmd(0xC6);				
	lcd_send_data(0X01);			
	/*电源控制1		AVDD 6.8V	AVCL -4.8V 	VDS 2.3V*/
	lcd_send_cmd(0xd0);				//Power Control 1
	lcd_send_data(0xa4);
	lcd_send_data(0xa1);
	/*电源控制2		SBCLK DIV 2   BCLK DIV 6*/
	lcd_send_cmd(0xE8);				//Power Control 1
	lcd_send_data(0x03);
	/*平衡时间控制*/
	lcd_send_cmd(0xE9);				
	lcd_send_data(0x09);
	lcd_send_data(0x09);
	lcd_send_data(0x08);
	/*正电平γ控制*/
	lcd_send_cmd(0xE0); 
	lcd_send_data(0xD0);
	lcd_send_data(0x05);
	lcd_send_data(0x09);
	lcd_send_data(0x09);
	lcd_send_data(0x08);
	lcd_send_data(0x14);
	lcd_send_data(0x28);
	lcd_send_data(0x33);
	lcd_send_data(0x3F);
	lcd_send_data(0x07);
	lcd_send_data(0x13);
	lcd_send_data(0x14);
	lcd_send_data(0x28);
	lcd_send_data(0x30);
	/*负电平γ控制*/
	lcd_send_cmd(0XE1); 
	lcd_send_data(0xD0);
	lcd_send_data(0x05);
	lcd_send_data(0x09);
	lcd_send_data(0x09);
	lcd_send_data(0x08);
	lcd_send_data(0x03);
	lcd_send_data(0x24);
	lcd_send_data(0x32);
	lcd_send_data(0x32);
	lcd_send_data(0x3B);
	lcd_send_data(0x14);
	lcd_send_data(0x13);
	lcd_send_data(0x28);
	lcd_send_data(0x2F);
	/*关闭显示翻转*/
	lcd_send_cmd(INVON); 		
	osal_msleep(120);
	/*开启显示*/
	lcd_send_cmd(0x29);         //开启显示
}


void spi_lcd_clear(uint16_t color)
{
	uint8_t color1,color2;
	lcd_send_cmd(0x2a);     //Column address set
	lcd_send_data(0x00);    //start column
	lcd_send_data(0x00); 
	lcd_send_data(0x01);    //end column
	lcd_send_data(0x40);

	lcd_send_cmd(0x2b);     //Row address set
	lcd_send_data(0x00);    //start row
	lcd_send_data(0x00); 
	lcd_send_data(0x00);    //end row
	lcd_send_data(0xf0);
	lcd_send_cmd(0x2C);     //Memory write

	color1 = color>>8;
	color2 = color;
	color = color2<<8|color1;
	SPI_DC_1;
	LCD_CS_0;
    for(int i=0;i<320 * 240;i++)
    {

		sw_spi_transfer_byte(color1);
        sw_spi_transfer_byte(color2);
        
    }
	LCD_CS_1;
}

void spi_lcd_draw_char(uint16_t x,uint16_t y,uint16_t color_text,uint16_t color_back,uint16_t *pdata)
{
  uint16_t column,raw;
	uint16_t temp;
	uint8_t data[768];

  lcd_send_cmd(0x2A);    //Column address set
  lcd_send_data(x>>8);    //start column
  lcd_send_data(x);
  x=x+15;
  lcd_send_data(x>>8);    //end column
  lcd_send_data(x);

  lcd_send_cmd(0x2b);     //Row address set
  lcd_send_data(y>>8);    //start row
  lcd_send_data(y); 
  y=y+23;
  lcd_send_data(y>>8);    //end row
  lcd_send_data(y);
  lcd_send_cmd(0x2C);     //Memory write
	for(raw=0;raw<24;raw++)
	{
		temp=pdata[raw];
		for(column=0;column<16;column++)
		{
			if(temp&0x01)
			{
				data[2*column+raw*32]=(color_text>>8);
				data[2*column+raw*32+1] = (color_text);
			}
			else 
			{
				data[2*column+raw*32]=(color_back>>8);
				data[2*column+raw*32+1] = (color_back);
			}
			temp>>=1;			
		}
	}
	TFT_SEND_DATA_NOT_SINGLE(data,768);
}

void spi_lcd_display_string_line(uint16_t x,uint16_t line,uint16_t color_text,uint16_t color_back,uint8_t *ptr)
{
	uint16_t i,data_length;
	uint16_t y_position = line * 24;
	for(i=0;i<20;i++)
	{
		if(*ptr == 0 )
		{
			break;
		}
		data_length = *ptr-32;
		spi_lcd_draw_char(x*16+16*i,y_position,color_text,color_back,ASCII_Table+data_length*24);
		ptr++;
	}
}

void spi_lcd_set_display_window(uint16_t xs,uint16_t xe)
{
	lcd_send_cmd(PTLON);
	lcd_send_cmd(PTLAR);
	lcd_send_data(xs>>8);    //start column
  	lcd_send_data(xs);
  	lcd_send_data(xe>>8);    //end column
  	lcd_send_data(xe);
	
}

void spi_lcd_window_mode_disable(void)
{
	lcd_send_cmd(MORON);
}

void spi_lcd_scrolling(void)
{
	lcd_send_cmd(VSCRDEF);
	lcd_send_data(0X00);    //start column
	lcd_send_data(0X00);
	lcd_send_data(0X01);    //start column
	lcd_send_data(0X40);	
	lcd_send_data(0X00);    //start column
	lcd_send_data(0X00);	
	lcd_send_cmd(VSCRSADD);
	lcd_send_data(0X00);    //start column
	lcd_send_data(0X00);
}



