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
 * @file   bsp_ili9341_4line.h
 * @brief  2.8寸屏ili9341驱动文件，采用4线SPI
 *
 ******************************************************************************
 */
#ifndef BSP_LCD_H
#define BSP_LCD_H

#include "gpio.h"
#include "pinctrl.h"
#include "osal_debug.h"
#include "osal_task.h"

/* LCD color */
#define RED          	 0XF800	  //红色
#define GREEN       	 0X07E0	  //绿色
#define WHITE       	 0XFFFF	  //白色
#define BLACK          0x0000
#define GREY           0xF7DE
#define BLUE           0x001F
#define BLUE2          0x051F
#define MEGENTA        0xF81F
#define CYAN           0x7FFF
#define YELLOW         0xFFE0


/***********COMMAND************/
#define SWRESET 		0X01	//软复位

#define SLPIN				0X10	//进入睡眠模式
#define SLPOUT			0X11	//退出睡眠模式
#define PTLON				0X12	//启动部分显示模式，由0X30设置区域
#define MORON				0X13	//启动正常显示模式

#define INVOFF			0X20	//关闭显示翻转
#define INVON				0X21	//启动显示翻转

#define GAMSET			0X26	//此命令用于选择当前显示所需的伽马曲线。说明最多可选择4条曲线。通过在参数中设置适当的位来选择曲线，具体操作请参见表。

#define DISPOFF			0X28	//关闭显示
#define DISPON			0X29	//开启显示
#define CASET				0X2A	//列地址设置
#define	RASET				0X2B	//行地址设置
#define RAMWR				0X2C	//memory写入

#define PTLAR				0X30	//部分显示模式的起始/结尾地址设置

#define VSCRDEF			0X33	//设置垂直滚动的区域
#define	TEOFF				0X34	//该命令用于关闭TE信号线的撕裂效应输出信号
#define	TEON				0X35	//撕裂影响线开启
#define MADCTL			0X36	//设置读写扫描方向
/*
				bit 7 ：	MY		页地址顺序  0：从上到下		1：从下到上
				bit 6 ：	MX    列地址顺序	0：从左到右		1：从右到左
				bit 5 ：	MV		页面/列顺序	0：正常模式		1：保留
				bit 4 ：	ML		行地址顺序	0：从上到下刷新	1：从下到上刷新
				bit 3 ：	RGB		色彩数据顺序	0：RGB	1:BGR
				bit 2 ：	MH		显示数据锁存数据顺序  0：从左到右刷新 1：从右到左刷新
*/

#define	VSCRSADD		0X37	//垂直滚动屏幕开始地址
#define IDMOFF			0X38	//空闲模式关闭
#define IDMON				0X39	//空闲模式开启，颜色表达会变少
#define COLMOD 			0X3A	//接口模式以及接口的像素模式 

#define	RAMWRC			0X3C	//memory持续写入	，控制写数据的方向？

#define TESCAN			0X44	//设置撕裂线
#define	WRDISBV			0X51	//设置显示亮度
#define	WRCTRLD			0X53	//写CTRL显示
/*
				bit 5 ：	BCTRL		控制亮度寄存器		0：直接置00 	1：亮度寄存器使能
				bit 3 ：  DD			显示调光       		0：关闭				1：开启调光
				bit 2	：	BL			控制背光					0：关闭				1：开启背光
*/
#define WRCACE			0X55	//编写内容自适应亮度控制和颜色增强
/*
				bit 0..1 ：C0/C1 内容自适应图像功能 	00：关闭 	01：用户接口模式 02：静止图片 03：动态图片
				bit 4..5 ：CE0/CE1	设置颜色增强级别	00：低  	01：中 		03：高
				bit 7 ：CECTRL 颜色增强控制	0：关闭 1：开启
*/
#define	WRCABCMB		0X5E	//写CABC最小亮度

#define RAMCTRL			0XB0	//RAM控制
/*
	param 1 ：
				bit 0..1 ： DM0/DM1	显示操作模式 00：MCU接口 01：RGB接口 02：VSYNC接口 03：保留
				bit 4 ：RM	RAM访问选择 0：MCU接口访问 1：RGB接口访问
	param 2 ：
				bit 0..1 ：MDT0/MDT1	像素数据的传输方法
				bit 2 ：	RIM  指定RGB总线的宽度 0：18位  1： 6位
				bit 3 ：	ENDIAN	大小端模式 0：大端模式MSB在前  1：小端模式LSB在前
				bit 4..5 ：EPF0/EPF1 将4K数据与65K数据转换成帧格式
*/
#define RGBCTRL			0XB1	//RGB控制
/*
	param 1 ：
				bit 7 ：WO 直接RGB模式 0：Memory 1：移位寄存器
				bit 5..6 ： RCM0/RCM1 RGB I/F使能模式选择 00/01：MCU接口 10：RGB DE 模式 11：RGB HV模式
				bit 3 ：VSPL	设置垂直同步引脚的信号极性。 0：低电平 1：高电平
				bit 2 ：HSPL	设置水平同步引脚的信号极性。 0：低电平 1：高电平
				bit 1 ：DPL		设置DOTCLK引脚的信号极性		 0：低电平 1：高电平
				bot 0 ：EPL 	设置enable引脚的信号极性		 0：低电平 1：高电平
	param 2 ：
				bit 0..6 ：VBP0..VBP6	RGB接口垂直同步后门廊设置。最小设置为0x02
	param 3 ：
				bit 0..4 ：HBO0..HBP4 RGB接口水平同步后门廊设置。请参阅第8.9.3节
*/
#define PORCTRL			0XB2	//行距控制
/*
	param 1 ：
				bit 0..6 ：BPA0..BPA6 后门廊设置在正常模式。最小设置为0x01。
	param 2 ：
				bit 0..6 ：FPA0..FPA6 前门廊设置在正常模式。最小设置为0x01。
	param 3 ：
				bit 0 ：PSEN 使能单独的门廊控制 0：禁用 1：使能
	param 4 ：
				bit 0..3 ：FPB0..FPB3 前门廊设置在空闲模式。最小设置为0x01。
				bit 4..7 ：BPB0..BPB3 后门廊设置在空闲模式。最小设置为0x01。
	param 5 ：
				bit 0..3 ：FPC0..FPC3 前门廊设置在部分模式。最小设置为0x01。
				bit 4..7 ：BPC0..BPC3 后门廊设置在部分模式。最小设置为0x01。
*/
#define FRCTRL1			0XB3	//帧速率控制
/*
	param 1 ：
				bit 4 ： FRSEN 使能单独的帧速率控制 0：禁用 1：使能
				bit 0..1 ：DIV0/DIV1 帧速率分频控制 00：1分频   01： 2分频  10： 4分频 	11： 8分频；
	param 2 ：
				bit 5..7 ：NLB0..NLB2  空闲模式下的反色选择   00 ：点翻转  07：列翻转
				bit 0..4 ：RTNB0..RTNB4 	空闲模式下的帧速率选择
				RTNB[4:0] FR in idle mode (Hz) 	RTNB[4:0] FR in idle mode (Hz)
				00h 			119 									10h 			58
				01h 			111 									11h			 	57
				02h				105 									12h 			55
				03h 			99 										13h 			53
				04h 			94 										14h 			52
				05h 			90 										15h 			50
				06h 			86 										16h 			49
				07h 			82 										17h 			48
				08h 			78 										18h			 	46
				09h 			75 										19h 			45
				0Ah 			72 										1Ah 			44
				0Bh 			69 										1Bh 			43
				0Ch 			67 										1Ch 			42
				0Dh 			64 										1Dh 			41
				0Eh 			62 										1Eh 			40
				0Fh 			60 										1Fh 			39
	param 3 ：
				bit 5..7 ：NLC0..NLC2  部分模式下的反色选择   00 ：点翻转  07：列翻转
				bit 0..4 ：RTNC0..RTNC4 	部分模式下的帧速率选择 同上表
*/
#define PARCTRL			0XB5	//部分显示控制
/*
				bit 7 ：NDL 部分显示模式下非显示区域的输出电平   	0：V63  1:V0
				bit 4 ：PTGISC 部分显示模式下非显示区域的扫描模式 0：正常扫描 1：间隔扫描
				bit 0..3：ISC0..ISC3 间隔扫描的频率
				ISC[3:0] 	Scan cycle for non-display area
				00h 			Normal scan
				01h 			Every 3 cycles scan 1 time
				02h 			Every 5 cycles scan 1 time
				03h 			Every 7 cycles scan 1 time
				…..
				…..
				0Fh 			Every 31 cycles scan 1 time
*/
#define GCTRL				0XB7	//门控制
/*
				bit 4..6 ：VGHS0..VGSH2 VGH设置
				VGHS[2:0] 	VGH (V)
				00h 				12.2
				01h 				12.54
				02h 				12.89
				03h 				13.26
				04h 				13.65
				05h 				14.06
				06h 				14.5
				07h 				14.97
				bit 0..2 ：VGLS0..VGLS2 VGL设置
				VGLS[2:0] 	VGL (V)
				00h		 			-7.16
				01h 				-7.67
				02h 				-8.23
				03h 				-8.87
				04h 				-9.6
				05h 				-10.43
				06h 				-11.38
				07h 				-12.5				
*/
#define GTADJ				0XB8	//增益控制调整	
/*
	param 1 ： 0X2A
	param 2 ： 0X2B
	param 3 ：
				bit 0..5 ： GTA0..GTA5 门开定时调整
	param 4 ：
				bit 4..7 ： GOFR0..GOFR3 仅用于RGB接口的关断定时调整
				bit 0..3 ： GOF0..GOF3  门关定时调整
*/
#define	DGMEN				0XBA	//数字γ使能
/*
				bit 2 ： 0：禁用数字γ  1：使能数字γ
*/
#define VCOMS				0XBB	//VCOM设置
/*
				bit 0..5 ：VCOM0..VCOM5
				VCOMS[5:0] 	VCOM (V) 	VCOMS[5:0]	VCOM (V)
				00h				 	0.1 			20h 				0.9
				01h 				0.125 		21h 				0.925
				02h 				0.15 			22h 				0.95
				03h 				0.175 		23h				 	0.975
				04h 				0.2 			24h 				1.0
				05h 				0.225 		25h 				1.025
				06h 				0.25			26h 				1.05
				07h 				0.275 		27h 				1.075
				08h 				0.3 			28h 				1.1
				09h 				0.325 		29h 				1.125
				0Ah 				0.35 			2Ah 				1.15
				0Bh 				0.375 		2Bh 				1.175
				0Ch 				0.4 			2Ch 				1.2
				0Dh 				0.425 		2Dh 				1.225
				0Eh 				0.45 			2Eh 				1.25
				0Fh 				0.475 		2Fh 				1.275
				10h 				0.5 			30h 				1.3
				11h 				0.525 		31h 				1.325
				12h 				0.55 			32h 				1.35
				13h 				0.575 		33h 				1.375
				14h 				0.6 			34h 				1.4
				15h 				0.625 		35h 				1.425
				16h 				0.65 			36h 				1.45
				17h 				0.675 		37h 				1.475
				18h 				0.7 			38h 				1.5
				19h 				0.725 		39h 				1.525
				1Ah 				0.75 			3Ah 				1.55
				1Bh 				0.775 		3Bh 				1.575
				1Ch 				0.8 			3Ch 				1.6
				1Dh 				0.825 		3Dh 				1.625
				1Eh 				0.85 			3Eh				 	1.65
				1Fh 				0.875 		3Fh 				1.675
*/
#define POWSAVE			0XBC	//省电模式
/*
				bit 1 ： NS  	正常模式下的省电
				bit 0 ： IS		空闲模式下的省电
*/
#define DLPOFFSAVE	0XBD	//显示关闭省电
/*
				bit 0 ： DOFSAVE 在显示关闭模式下节省功耗 
*/
#define	LCMCTRL			0XC0	//LCM控制
/*
				bit 6 ：XMY	36h中的XOR MY设置。
				bit 5 ：XBGR 36h命令中的XOR RGB设置。
				bit 4 ：XREV	命令21h中的异或反设置
				bit 3 ：XMH	该位可以反转源输出顺序，只支持RGB接口，不支持RAM模式
				bit 2 ：XMV	命令36h中的XOR MV设置
				bit 1 ：XMX	命令36h中的XOR MX设置。
				bit 0 ：XGS	E4h命令中的XOR GS设置
*/
#define	IDSET				0XC1	//ID设置
#define	VDVVRHEN		0XC2	//VDV和VRH命令使能
/*
bit 0 ：CMDEN  VDV和VRH命令写使能  0 ：VDV和VRH寄存器的值来自于NVM  1:VDV和VRH寄存器的值来自于命令
*/
#define	VRHS				0XC3	//VRH设置
#define	VDVSET			0XC4	//VDV设置
#define VCMOFSET		0XC5	//VCOM偏移设置
#define	FRCTR2			0xC6	//正常模式下的帧速率控制
#define	CABCCTRL		0xC7	//CABC控制
/*
				bit 3 ：LEDONREV 翻转LED的灯亮状态 0：保持原状 1：翻转LED_ON的状态
				bit 2 ：DPOFPWM 初始化ledpwm控制状态 0：初始化LEDPWM的状态为低 1：初始化LEDPWM的状态为高
				bit 1 ：PWMFIX LEDPWM修复控制	0：LEDPWM由CABC控制  1：修正LEDPWM在ON状态
				bit 0 ：PWMPOL LEDPWM极性控制 0：高	1：低
*/
#define REGSEL1			0XC8	//寄存器值选择1
#define REGSEL2			0XCA	//寄存器值选择2
#define PWMFRSEL		0XCC	//PWM频率选择
#define	PWCTRL1			0XD0	//电源控制1
#define VAPVANEN		0XD2	//使能VAP/VAN信号输出
#define	CMD2EN			0XDF	//0X5A  0X69 	0X02	命令2使能
#define	PVGAMCTRL		0XE0	//高电平γ控制
#define NVGAMCTRL		0XE1	//低电平γ控制
#define	DGMLUTR			0XE2	//红色数字伽马查表
#define	DGMLUTB			0XE3	//蓝色数字伽马查表
#define	GATECTRL		0XE4	//阀门控制
#define	SPI2EN			0XE7	//SPI2使能
#define	PWCTRL2			0XE8	//电源控制2
#define EQCTRL			0XE9	//平衡时间控制
#define	PROMCTRL		0XEC	//编程控制
#define	PROMEN			0XFA	//编程模式使能
#define	NVMSET			0XFC	//NVM设置
#define	PROMACT			0XFE	//编程行为




void spi_lcd_init(void);

void spi_lcd_clear(uint16_t color);

void spi_lcd_window_mode_disable(void);
void spi_lcd_set_display_window(uint16_t xs,uint16_t xe);
void spi_lcd_display_string_line(uint16_t x,uint16_t line,uint16_t color_text,uint16_t color_back,uint8_t *ptr);
void spi_lcd_draw_char(uint16_t x,uint16_t y,uint16_t color_text,uint16_t color_back,uint16_t *pdata);
void spi_lcd_scrolling(void);

#endif /* __BSP_ili9341_4LINE_H__ */
