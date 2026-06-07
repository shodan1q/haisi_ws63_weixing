# haisi_ws63_weixing · 温湿度变送器 + MQTT（branch temp-humid-sensor）

WS63 个人开发分仓 —— **温湿度 + 气压变送器，含 WiFi/MQTT 远程上报**。

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

## MQTT 话题（Tasmota 风格）

### Publish（板子→外部）

每 **5 秒** 自动上报：

| Topic | 内容 | Retained |
|---|---|---|
| `tele/ws63_sensor/SENSOR` | JSON `{"temp":25.6,"humid":65.3,"press":1013.2}` | ✓ |
| `tele/ws63_sensor/temperature` | `"25.6"` | ✓ |
| `tele/ws63_sensor/humidity` | `"65.3"` | ✓ |
| `tele/ws63_sensor/pressure` | `"1013.2"` | ✓ |

Retained=1 表示订阅者一连上 broker 就立刻收到最新值，不用等下一次刷新。

### Subscribe（外部→板子）

| Topic | payload | 行为 |
|---|---|---|
| `cmnd/ws63_sensor/get` | 任意 | 立即把当前读数发到 `stat/ws63_sensor/SENSOR`（JSON 同上） |

### 测试命令（电脑装了 mosquitto）

```bash
# 订阅所有遥测话题
mosquitto_sub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t 'tele/ws63_sensor/#' -v

# 只看 JSON
mosquitto_sub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t 'tele/ws63_sensor/SENSOR'

# 立即触发一次上报，结果在 stat 话题
mosquitto_sub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t 'stat/ws63_sensor/SENSOR' &
mosquitto_pub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t cmnd/ws63_sensor/get -m '1'
```

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

```
$ mosquitto_sub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t 'tele/ws63_sensor/#' -v
tele/ws63_sensor/SENSOR {"temp":25.6,"humid":65.3,"press":1013.2}
tele/ws63_sensor/temperature 25.6
tele/ws63_sensor/humidity 65.3
tele/ws63_sensor/pressure 1013.2
```

每 5 秒刷新一次。

---

## 想改什么

| 想改 | 改这里 |
|---|---|
| WiFi 账密 | `app_demo.c` 顶部 `WIFI_SSID` / `WIFI_PWD` |
| 上报周期（默认 5 s） | `app_demo.c` 的 `MQTT_PUBLISH_PERIOD_MS` |
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
| `net/sensors_mqtt.{c,h}` | Paho MQTT 包装，Tasmota 风格 topic |
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
