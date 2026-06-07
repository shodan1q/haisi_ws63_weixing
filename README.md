# haisi_ws63_weixing · 温湿度变送器 + MQTT（branch temp-humid-sensor）

WS63 个人开发分仓 —— **温湿度 + 气压变送器，含 WiFi/MQTT 请求-响应接口**。

| 模块 | 总线 / 引脚 | 用途 |
|---|---|---|
| **SHT30** | I2C 0x44，软件 I2C，SDA=GPIO_7 / SCL=GPIO_8 | 温度 + 相对湿度 |
| **BMP280** | I2C 0x76，**和 SHT30 共用同一条总线** | 温度 + 大气压 |
| **TM1640** 7 段数码管 | 2 线串行，DIN=GPIO_14 / CLK=GPIO_13 | 显示温度/湿度/气压轮播 |
| LCD | 已有 SPI 接口 | 完整状态 + 各传感器/网络状态 |
| **WiFi + MQTT** | 板载 WiFi | 上报 + 远程拉取 |

挂载点 `src/application/samples/custom/`，SDK 源码零修改。

---

## 网络配置

| 项 | 值 |
|---|---|
| WiFi SSID | `NBeeNET` |
| WiFi 密码 | `nbeenet88888888` |
| MQTT Broker | `tcp://121.41.23.138:1883` |
| MQTT Client ID | `ws63-sensor-a1`（确保和手表 `weixing-a1`、舵机板 `weixing-ws63` 都不同） |
| 用户名 / 密码 | `public` / `Aa123456` |

WiFi 账密在 `app_demo.c` 顶部 `#define WIFI_SSID` / `#define WIFI_PWD` 改。

---

## MQTT 话题（请求-响应模式）

> **完整接入文档：[`MQTT_API.md`](MQTT_API.md)**（含 Python / Node.js / Home Assistant 示例和故障排查）

板子**不会主动定时上报**——只有当 broker 上有人发 `cmnd/ws63_sensor/get` 时，才推送一次。所有 publish 都是 **非 retained**，单纯订阅 `stat/...` 不会自动收到旧值，必须先 `cmnd/get` 一下。

### 触发上报（外部→板子）

| Topic | payload |
|---|---|
| `cmnd/ws63_sensor/get` | 任意（`1`、`now`、`{}` 都行）|

### 响应（板子→外部）

收到 cmd 后立即推送 4 条 `stat/...` 消息：

| Topic | 内容 |
|---|---|
| `stat/ws63_sensor/SENSOR` | JSON `{"temp":25.6,"humid":65.3,"press":1013.2}` |
| `stat/ws63_sensor/temperature` | `25.6` |
| `stat/ws63_sensor/humidity` | `65.3` |
| `stat/ws63_sensor/pressure` | `1013.2` |

### 测试命令（电脑装了 mosquitto）

```bash
# 终端 1：订阅响应（先开着，平时是静默的）
mosquitto_sub -h 121.41.23.138 -p 1883 -u public -P Aa123456 \
              -t 'stat/ws63_sensor/#' -v

# 终端 2：每次想拿数据就发一次 get
mosquitto_pub -h 121.41.23.138 -p 1883 -u public -P Aa123456 \
              -t cmnd/ws63_sensor/get -m 1
```

每发一次 `cmnd/get`，终端 1 立刻打印 4 条 `stat/...` 消息。如果不发，终端 1 始终静默。

---

## 接线

### SHT30 + BMP280（共享 I2C）

| 信号 | 板上 |
|---|---|
| SDA | **GPIO_7** |
| SCL | **GPIO_8** |
| VCC | 3.3V |
| GND | GND |

两片传感器**并联**在同一对线上（I2C 总线特性，地址不同）。传感器小板自带 4.7k–10k 上拉电阻，不要再外接。

### TM1640 7 段数码管

| 信号 | 板上 |
|---|---|
| DIN (模块标 SDA) | **GPIO_14** |
| CLK (模块标 SCL) | **GPIO_13** |
| VCC | 5V |
| GND | GND |

---

## Windows 编译

```powershell
cd D:\fbb_ws63-master\src\application\samples\custom
git checkout temp-humid-sensor
git pull
# git log -1 应该是最新的 mqtt commit

# HiSpark Studio: Clean → Build → 烧录 → 复位
```

---

## 预期效果

### 串口

```
[boot] sensor_task started
[boot] sht30 probe rc=0
[boot] bmp280_init rc=0
[boot] sensor_task entering main loop
[net] connecting WiFi SSID=NBeeNET ...
[WIFI_STA_SAMPLE]::Connect succ!.
[net] WiFi connected
[mqtt] connected to tcp://121.41.23.138:1883 as ws63-sensor-a1
[mqtt] subscribed cmnd/ws63_sensor/get
[loop 5] sht30 rc=0 T=25.6 H=65.3 bmp rc=0 P=1013.2 ...
```

### LCD

```
WS63 T/H/P sensor
SDA7/SCL8 Tube DIN14/CLK13
                       
Temp:    25.6 C         (绿)
Humid:   65.3 %         (绿)
Press: 1013.2 hPa       (白)
                       
ok S:N B:N err:0/0
WiFi:OK  MQTT:OK        (绿)
```

### 数码管

每 3 秒切换 `25.6 65.3` ↔ `P 1013.2`。

### MQTT 订阅端能看到

平时静默。每发一次 `cmnd/ws63_sensor/get`，立刻出 4 条：

```
$ mosquitto_sub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t 'stat/ws63_sensor/#' -v
stat/ws63_sensor/SENSOR {"temp":25.6,"humid":65.3,"press":1013.2}
stat/ws63_sensor/temperature 25.6
stat/ws63_sensor/humidity 65.3
stat/ws63_sensor/pressure 1013.2
```

---

## 想改什么

| 想改 | 改这里 |
|---|---|
| WiFi 账密 | `app_demo.c` 顶部 `WIFI_SSID` / `WIFI_PWD` |
| 设备名（topic root） | `net/sensors_mqtt.c` 的 `DEV_ID` (`ws63_sensor`) |
| Broker / 账密 | `net/sensors_mqtt.c` 顶部 `MQTT_*` 宏 |
| Client ID | `MQTT_CLIENTID`（必须独一，不能撞手表/舵机板） |

---

## 文件清单

| 文件 | 说明 |
|---|---|
| `app_demo.c` | 主入口：LCD/sensor/net 三个任务 |
| `sensors/i2c_bb.{c,h}` | 软件 I2C |
| `sensors/sht30.{c,h}` | SHT30 单次读 + CRC8 |
| `sensors/bmp280.{c,h}` | BMP280 校准 + Bosch 补偿 |
| `sensors/tm1640.{c,h}` | TM1640 7 段驱动 + ASCII→段码 |
| `net/wifi_connect.{c,h}` | 阻塞式 WiFi STA 连接 |
| `net/sensors_mqtt.{c,h}` | Paho MQTT 包装，请求-响应模式 |
| `lcd.c` / `lcd.h` / `fonts.c` / `fonts.h` | ILI9341 LCD |

---

## 分支总览

| 分支 | 内容 |
|---|---|
| `main` | 双舵机 + 激光灯 + MQTT |
| `sle-speed-backup` | 两板 SLE 通信 |
| `nfc-5321` | PN532 NFC 读卡 |
| **`temp-humid-sensor`** | 当前：SHT30+BMP280+TM1640+WiFi+MQTT |

---

## 上游

- WS63 SDK 上游：https://gitee.com/HiSpark/fbb_ws63
- MQTT broker 同 esp32watch 项目
- Tasmota-style topic 参考：https://tasmota.github.io/docs/MQTT/
