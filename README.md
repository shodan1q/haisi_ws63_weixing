# haisi_ws63_weixing

WS63 个人开发分仓 —— **4T_HRM_QS2 综合演示**：

| 外设 | 行为 |
|---|---|
| LCD (ILI9341, 软件 SPI) | 顶行 "Hello WS63"，按键按下显示对应消息，底行显示运行时长 |
| 3 颗 LED (红/绿/黄) | 同步闪烁，周期 1 秒 |
| 蜂鸣器 (PWM) | 每 10 秒响一次，~150ms |
| 4 个按键 (ADC 多路复用) | KEY1~4 各显示一条不同消息 + 串口打印 |

挂载点 `src/application/samples/custom/`，**SDK 源码零修改**。

---

## Windows 上怎么用

```powershell
# 第一次：clone 到 SDK 扩展点
cd D:\fbb_ws63-master\src\application\samples
git clone https://github.com/shodan1q/haisi_ws63_weixing.git custom

# 已经 clone 过：拉新代码
cd D:\fbb_ws63-master\src\application\samples\custom
git pull

# HiSpark Studio：
#   Kconfig → Application → ✓ Enable Sample → Save
#   Clean → Build → 烧录 → 按 RST
```

预期：
- 串口前几行有 boot 日志，按键时会看到 `KEY1 pressed` ~ `KEY4 pressed`
- LCD 顶行黑底白字 `Hello WS63`
- 红 / 绿 / 黄三个 LED 同步闪
- 大约每 10 秒蜂鸣器响一短声

---

## 引脚（来自 4T_HRM_QS2 协议文档）

| 外设 | GPIO | 备注 |
|---|---|---|
| LCD WR/DC | GPIO_03 | 由 `lcd.c` 驱动 |
| LCD CS | GPIO_05 | 同上 |
| SPI MOSI/MISO/CLK | GPIO_01 / 04 / 06 | 软件 SPI |
| LED1 红 | GPIO_10 | `app_demo.c` 中 `LED_RED_PIN` |
| LED2 绿 | GPIO_11 | `LED_GREEN_PIN` |
| LED3 黄 | GPIO_12 | `LED_YELLOW_PIN` |
| 蜂鸣器 | GPIO_07 | PWM 通道 7 |
| ADC 按键 | GPIO_08 | ADC ch1，4 键电压分压 |

按键电压判定（来自板厂原 demo，已实测）：

| 电压 (mV) | 键 |
|---|---|
| ≤ 500 | KEY1 |
| 1000 ~ 2000 | KEY2 |
| 2100 ~ 2500 | KEY3 |
| 2600 ~ 2800 | KEY4 |

---

## 想改什么

| 想改 | 改这里 |
|---|---|
| LCD 显示内容 | `app_demo.c` `lcd_task()` 里的字符串，按键消息在 `key_messages[]` |
| LED 闪烁速度 | `LED_HALF_PERIOD_MS`（500=1Hz） |
| 蜂鸣器频次 | `BUZZ_INTERVAL_MS` (10000=10s) / `BUZZ_DURATION_MS` |
| LED 改为轮流亮 | `led_task()` 改成 for 循环逐个点亮 |
| 按键动作变成切换颜色 | `lcd_task()` 里根据 key id 改 `spi_lcd_clear()` 颜色 |

---

## 文件清单

| 文件 | 说明 |
|---|---|
| `app_demo.c` | 主入口，4 个任务（LCD/LED/Buzzer/ADC button）+ `app_run` |
| `lcd.c` / `lcd.h` | ILI9341 4 线软件 SPI 驱动（板厂原版） |
| `fonts.c` / `fonts.h` | ASCII 字库（板厂原版） |
| `CMakeLists.txt` | 把 `app_demo.c + lcd.c + fonts.c` 接进 samples 组件 |

---

## 上游
- WS63 SDK 上游：https://gitee.com/HiSpark/fbb_ws63
- 完整 SDK 镜像：https://github.com/shodan1q/haisi_ws63
- 板厂资料：4T_HRM_QS2（华清远见）
