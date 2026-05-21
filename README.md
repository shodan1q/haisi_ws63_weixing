# haisi_ws63_weixing · 温湿度变送器（branch temp-humid-sensor）

WS63 SDK 个人开发分仓 —— **温湿度 + 气压变送器**。

| 模块 | 总线 / 引脚 | 用途 |
|---|---|---|
| **SHT30** | I2C 0x44，软件 I2C，SDA=GPIO_11 / SCL=GPIO_12 | 温度 + 相对湿度 |
| **BMP280** | I2C 0x76，**和 SHT30 共用同一条总线** | 温度 + 大气压 |
| **TM1640** 7 段数码管 | 2 线串行，DIN=GPIO_13 / CLK=GPIO_3 | 显示温度/湿度（每 2 秒交替） |
| LCD | 已有 SPI 接口 | 完整状态 + 各传感器 ok/err 计数 |

挂载点 `src/application/samples/custom/`，SDK 源码零修改。

---

## 接线

### SHT30 + BMP280（共享一条 I2C 总线）

| 信号 | 接板上 |
|---|---|
| SDA | **GPIO_11** |
| SCL | **GPIO_12** |
| VCC | 3.3V |
| GND | GND |

两片传感器**并联在同一对线上**（I2C 是共享总线），3.3V 和 GND 直连。各传感器小板上通常已经焊好 4.7k–10k 上拉到 VCC，**不要再外接**。

### TM1640 7 段数码管

| 信号 | 接板上 |
|---|---|
| DIN | **GPIO_13** |
| CLK | **GPIO_3** |
| VCC | 5V（多数 TM1640 数码管要 5V，看你模块标的） |
| GND | GND |

⚠️ **TM1640 不是 I2C**，它是自有 2 线协议（无 ACK、LSB first），只能 bit-bang，不能挂任何 I2C 设备。

---

## Windows 切到本分支并编

```powershell
cd D:\fbb_ws63-master\src\application\samples\custom

git fetch
git checkout -b temp-humid-sensor origin/temp-humid-sensor

# HiSpark Studio: Clean → Build → 烧录 → 复位
```

---

## 预期运行效果

**LCD**：
```
WS63 T/H/P sensor
Bus1 SDA11/SCL12
Tube DAT13/CLK3
                       
Temp:   25.6 C         (绿)
Humid:  65.3 %         (绿)
Press: 1013.2 hPa      (白)
                       
ok S:N B:N err S:N B:N (各传感器累计成功/失败次数)
Uptime: N s
```

**数码管**（每 2 秒交替）：
- 偶数秒：`25.6 C ` （温度）
- 奇数秒：`65.3 H ` （湿度，H 代表 Humidity）

**串口** (UART0 / USB)：
```
[sensor] bmp280_init rc=0
[sensor] T=25.6 C  RH=65.3 %  P=1013.2 hPa  (sht30_ok=1 err=0, bmp_ok=1 err=0)
[sensor] T=25.6 C  RH=65.4 %  P=1013.2 hPa  (sht30_ok=2 err=0, bmp_ok=2 err=0)
...
```

---

## 排错速查

| 现象 | 含义 / 处理 |
|---|---|
| LCD `Temp: 0.0 C` 不变，`err S:N` 一直涨 | SHT30 没响应：检查接线、3.3V 通不通 |
| LCD 温度正常但 `Press: 0.0 hPa`、`err B:N` 涨 | BMP280 没响应：是否焊接好、是否真的是 0x76 地址（有些模组是 0x77） |
| 串口 `bmp280_init rc=-2` | 收到了 chip ID 但不是 0x58，可能是 BMP180 而不是 BMP280 |
| 串口 `bmp280_init rc=-1` | I2C 完全没通，看 SDA/SCL 接线 |
| 数码管全黑 | 5V 没接 / DIN/CLK 接反 / 亮度没设（代码里默认 4，可改成 8） |
| 数码管显示乱码 | 接错引脚 / CLK 太快——可在 `tm1640.c` 里把 `TM1640_BIT_DELAY_US` 加大 |

---

## API 用法

```c
#include "sensors/sht30.h"
#include "sensors/bmp280.h"
#include "sensors/tm1640.h"

/* I2C 总线 */
i2c_bb_t bus = { .sda_pin = 11, .scl_pin = 12 };
i2c_bb_init(&bus);

/* SHT30 直接读 */
float t, h;
if (sht30_read(&bus, &t, &h) == 0) {
    printf("T=%.1fC H=%.1f%%\n", t, h);
}

/* BMP280 — 先读校准 */
bmp280_calib_t cal;
bmp280_init(&bus, &cal);
float bt, p;
bmp280_read(&bus, &cal, &bt, &p);

/* 数码管 */
tm1640_t tube = { .clk_pin = 3, .data_pin = 13 };
tm1640_init(&tube);
tm1640_set_brightness(&tube, 4);   /* 0..8 */
tm1640_show_text(&tube, "25.6C");
```

`tm1640_show_text` 识别字符：`0..9` `-` `.` ` ` `C` `H` `F` `P` `E` `r`，其它显示为空白。

---

## 文件清单

| 文件 | 说明 |
|---|---|
| `app_demo.c` | 主入口：LCD 任务 + 1Hz 传感器任务 |
| `sensors/i2c_bb.{c,h}` | 软件 I2C（任意 GPIO 都能跑） |
| `sensors/sht30.{c,h}` | SHT30 单次读 + CRC8 校验 |
| `sensors/bmp280.{c,h}` | BMP280 chip ID + 24 字节校准 + Bosch 补偿算法 |
| `sensors/tm1640.{c,h}` | TM1640 7 段驱动 + ASCII→段码 |
| `lcd.c` / `lcd.h` / `fonts.c` / `fonts.h` | ILI9341 LCD 驱动 |

---

## 分支总览

| 分支 | 内容 |
|---|---|
| `main` | SG90 舵机平滑扫描 |
| `sle-speed-backup` | 两板 SLE 通信 |
| `nfc-5321` | PN532 NFC 读卡（UART1） |
| **`temp-humid-sensor`** | 当前：SHT30+BMP280+TM1640 |

---

## 参考

- 参考自用户给的 ESP32 实现（`sensor_sht30.c`, `bmp280_i2c.c`, `sx_tm1640.c`）
- WS63 SDK 上游：https://gitee.com/HiSpark/fbb_ws63
- 完整 SDK 镜像：https://github.com/shodan1q/haisi_ws63
