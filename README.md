# haisi_ws63_weixing · 双舵机 + MQTT 控制器

WS63 个人开发分仓 —— **两个 SG90 舵机（GPIO_8 / GPIO_9）+ WiFi + MQTT 远程控制**。

挂载点 `src/application/samples/custom/`，SDK 源码零修改。

---

## 功能

1. **开机两个舵机同时复位到中位 0°**（保持 0.6 秒）
2. 连接 WiFi（账密在 `app_demo.c` 里填）
3. 连接 MQTT broker（配置和 esp32watch 工程一致）
4. 订阅命令 topic，收到角度就**两个舵机同步转过去**（平滑扫动）
5. 定期上报当前角度

## MQTT 配置（与 esp32watch 一致）

| 项 | 值 |
|---|---|
| Broker | `tcp://121.41.23.138:1883` |
| Client ID | `weixing-a1` |
| 用户名 | `public` |
| 密码 | `Aa123456` |
| 订阅（舵机） | `sat/a1/servo` |
| 订阅（激光/红外灯） | `sat/a1/laser` |
| 发布（遥测） | `sat/a1/telemetry` |

- **舵机命令**：payload 是目标角度 ASCII 整数 -90 ~ +90。
  发 `45` → 两舵机同步转到 +45°；发 `-90` → -90°。
- **激光灯命令**：payload `1`/`on`/`ON`/`true` → 开；`0`/`off` → 关。
- **遥测**：每 3 秒往 `sat/a1/telemetry` 发 `{"angle":N,"laser":0/1}`。

测试命令（电脑上装了 mosquitto）：
```bash
# 舵机
mosquitto_pub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t sat/a1/servo -m 45
mosquitto_pub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t sat/a1/servo -m -90
# 激光/红外灯
mosquitto_pub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t sat/a1/laser -m on
mosquitto_pub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t sat/a1/laser -m off
# 看遥测
mosquitto_sub -h 121.41.23.138 -p 1883 -u public -P Aa123456 -t sat/a1/telemetry
```

---

## 接线

| 舵机 | 信号 | 板上 |
|---|---|---|
| 舵机 A | 橙色（信号） | **GPIO_8** |
| 舵机 B | 橙色（信号） | **GPIO_9** |
| 两个舵机 | 红色 VCC | 5V |
| 两个舵机 | 棕色 GND | GND |
| 红外/激光灯 | 三极管基极（经限流电阻） | **GPIO_10** |
| 红外/激光灯 | 三极管 + 灯供电 | 按你电路 |

⚠️ 两个 SG90 同时动，峰值电流可能 ~500mA。**强烈建议用外置 5V 电源**给舵机供电，和板子共地，不要全靠 USB 5V（容易掉电复位）。

---

## ⚠️ 烧录前必须改 WiFi 账密

编辑 `app_demo.c` 顶部：
```c
#define WIFI_SSID  "你的WiFi名"
#define WIFI_PWD   "你的WiFi密码"
```
WS63 只支持 **2.4GHz** WiFi，5GHz 连不上。

---

## Windows 编译

```powershell
cd D:\fbb_ws63-master\src\application\samples\custom
git checkout main
git pull

# 改 app_demo.c 的 WIFI_SSID / WIFI_PWD

# HiSpark Studio: Kconfig → Application → ✓ Enable Sample → Save
#                 Clean → Build → 烧录 → 复位
```

### 如果链接报 `undefined reference to MQTTClient_connect`

说明 MQTT 库（paho）没被链接进固件。需要在 Kconfig 里把 MQTT 相关开关打开。
通常在 Kconfig 里搜 `MQTT` / `open_source`，或参考 HiHope mqtt demo 的启用方式。
把报错贴回来我帮你定位具体开关。

---

## LCD 显示

```
WS63 Dual Servo+MQTT
Servo GPIO_8 / GPIO_9
                       
WiFi: CONNECTED        (绿)
MQTT: CONNECTED        (绿)
Angle: +45 deg
                       
Uptime: N s
```

---

## 串口日志

```
[servo] reset to center done
[net] connecting WiFi SSID=... 
[net] WiFi connected
[mqtt] connected to tcp://121.41.23.138:1883 as weixing-a1
[mqtt] subscribed sat/a1/cmd
[mqtt] cmd on sat/a1/cmd = '45'
[mqtt] servo target set to 45 deg
```

---

## 文件清单

| 文件 | 说明 |
|---|---|
| `app_demo.c` | 主入口：复位舵机 → WiFi → MQTT → 遥测循环 |
| `servo/servo_dual.{c,h}` | 双舵机同步驱动（GPIO_8/9 软件 PWM） |
| `net/wifi_connect.{c,h}` | 阻塞式 WiFi STA 连接（HiHope helper） |
| `net/mqtt_app.{c,h}` | Paho MQTT 客户端，esp32watch broker 配置 |
| `lcd.c` / `fonts.c` 等 | ILI9341 LCD 驱动 |

---

## 想改什么

| 想改 | 改这里 |
|---|---|
| Broker / 账密 / topic | `net/mqtt_app.c` 顶部 `MQTT_*` / `TOPIC_*` 宏 |
| 两个舵机各转不同角度 | `servo_dual.c` 把单一 `g_target_us` 拆成两个 + `emit_frame` 分别计时 |
| 转动速度 | `servo_dual.c` 的 `SWEEP_STEP_US`（越大越快） |
| 上报周期 | `app_demo.c` 的 `TELEMETRY_PERIOD_MS` |

---

## 分支总览

| 分支 | 内容 |
|---|---|
| **`main`** ⬅️ 当前 | 双舵机 + MQTT |
| `sle-speed-backup` | 两板 SLE 通信 |
| `nfc-5321` | PN532 NFC 读卡 |
| `temp-humid-sensor` | SHT30+BMP280+TM1640 温湿度 |

单舵机平滑扫描的旧版在 git history commit `9d05c75`。

---

## 上游

- WS63 SDK 上游：https://gitee.com/HiSpark/fbb_ws63
- MQTT 配置来源：`/Users/shodan/project/esp32watch` 的 `bsp_mqtt.c`
- WiFi/MQTT 参考：`vendor/HiHope_NearLink_DK_WS63E_V03/demo/mqtt/`
