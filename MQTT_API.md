# WS63 温湿度气压变送器 · MQTT 接入文档

> 适用固件分支：`temp-humid-sensor`
> 设备型号：HiSilicon WS63 + SHT30 + BMP280
> 通信模式：**请求-响应（Request-Response）** —— 设备**不主动上报**，客户端发请求才有数据

---

## 1. Broker 连接参数

| 项 | 值 |
|---|---|
| 地址 | `tcp://121.41.23.138:1883` |
| 协议 | MQTT 3.1.1（Paho 默认）|
| 用户名 | `public` |
| 密码 | `Aa123456` |
| Keep-Alive | 60 s |
| Clean Session | true |
| TLS | 否 |
| 设备 Client ID | `ws63-sensor-a1`（**勿与下方任何客户端的 Client ID 冲突**）|

> Client ID 在 MQTT 协议里必须全局唯一。客户端可随意取名，但**不能**用 `ws63-sensor-a1`、`weixing-a1`、`weixing-ws63` 这三个值（分别已被本设备、手表、舵机板占用），否则会把对方踢下线。

---

## 2. 主题清单

设备 ID 为 `ws63_sensor`，共 5 个主题：

| 方向 | 主题 | QoS | Retained | 说明 |
|---|---|---|---|---|
| **客户端 → 设备** | `cmnd/ws63_sensor/get` | 0 | 否 | 触发一次采样并响应。Payload 可为任意值（`1`、`now`、`{}` 都接受） |
| 设备 → 客户端 | `stat/ws63_sensor/SENSOR` | 0 | 否 | 完整 JSON 快照 |
| 设备 → 客户端 | `stat/ws63_sensor/temperature` | 0 | 否 | 仅温度，纯文本 `"25.6"` |
| 设备 → 客户端 | `stat/ws63_sensor/humidity` | 0 | 否 | 仅相对湿度，纯文本 `"65.3"` |
| 设备 → 客户端 | `stat/ws63_sensor/pressure` | 0 | 否 | 仅大气压（hPa），纯文本 `"1013.2"` |

**所有响应均为非 retained**。这意味着：
- 客户端**仅订阅**而不发请求，永远收不到数据。
- Broker 不缓存历史值，订阅时刻之前的响应都丢失。
- 每个 `cmnd/get` 对应**一组 4 条**响应消息（先 SENSOR，再三个单值）。

---

## 3. 响应数据格式

### 3.1 `stat/ws63_sensor/SENSOR`（推荐主用）

```json
{"temp":25.6,"humid":65.3,"press":1013.2}
```

| 字段 | 单位 | 量程 | 来源 |
|---|---|---|---|
| `temp`  | °C   | -40 ~ 125     | SHT30（也可读 BMP280 温度，固件取 SHT30）|
| `humid` | %RH  | 0 ~ 100       | SHT30 |
| `press` | hPa  | 300 ~ 1100    | BMP280（已做 Bosch 32-bit 补偿）|

所有数值固定保留 **1 位小数**。

### 3.2 单值主题

`stat/ws63_sensor/temperature` 等三个主题的 payload 是裸字符串，**不带引号**、**不带单位**，例如：

```
25.6
```

适合接入老式数据看板（Node-RED、Home Assistant `sensor.mqtt`、Tasmota 仪表盘）一对一映射。

---

## 4. 时序

```
客户端                                设备
  │                                    │
  │── PUB cmnd/ws63_sensor/get "1" ──→ │
  │                                    │── 立即触发 snapshot_cb
  │                                    │   读取最近一次 sensor_task 缓存
  │                                    │   （sensor_task 以 1 Hz 刷新）
  │                                    │
  │ ←── PUB stat/ws63_sensor/SENSOR ───│
  │ ←── PUB stat/.../temperature ──────│
  │ ←── PUB stat/.../humidity ─────────│
  │ ←── PUB stat/.../pressure ─────────│
  │                                    │
```

延迟参考：从发出 `cmnd/get` 到收到完整 4 条响应，**典型 50–200 ms**（取决于客户端到 broker 网络）。

注意：返回的是设备**最近一次**采样值（1 Hz 缓存），不是收到 cmd 那一刻才去读传感器，避免阻塞 MQTT 回调。所以最坏情况数据延迟最多 ~1 s。

---

## 5. 命令行示例（mosquitto）

```bash
# 终端 A：长开订阅
mosquitto_sub -h 121.41.23.138 -p 1883 \
              -u public -P Aa123456 \
              -i my-cli-sub-1 \
              -t 'stat/ws63_sensor/#' -v

# 终端 B：请求一次
mosquitto_pub -h 121.41.23.138 -p 1883 \
              -u public -P Aa123456 \
              -i my-cli-pub-1 \
              -t 'cmnd/ws63_sensor/get' -m 1
```

终端 A 立即出现：

```
stat/ws63_sensor/SENSOR {"temp":25.6,"humid":65.3,"press":1013.2}
stat/ws63_sensor/temperature 25.6
stat/ws63_sensor/humidity 65.3
stat/ws63_sensor/pressure 1013.2
```

---

## 6. Python 客户端示例（paho-mqtt）

```python
import json, time
import paho.mqtt.client as mqtt

BROKER = "121.41.23.138"
USER, PWD = "public", "Aa123456"
DEV = "ws63_sensor"

def on_message(client, userdata, msg):
    if msg.topic.endswith("/SENSOR"):
        data = json.loads(msg.payload)
        print(f"T={data['temp']} °C  H={data['humid']} %  P={data['press']} hPa")

c = mqtt.Client(client_id="py-demo-001", clean_session=True)
c.username_pw_set(USER, PWD)
c.on_message = on_message
c.connect(BROKER, 1883, 60)
c.subscribe(f"stat/{DEV}/SENSOR", qos=0)
c.loop_start()

while True:
    c.publish(f"cmnd/{DEV}/get", payload="1", qos=0, retain=False)
    time.sleep(5)        # 自己控制轮询频率
```

---

## 7. Node.js 客户端示例（mqtt.js）

```javascript
import mqtt from "mqtt";

const client = mqtt.connect("mqtt://121.41.23.138:1883", {
  username: "public",
  password: "Aa123456",
  clientId: "node-demo-001",
  clean: true,
});

client.on("connect", () => {
  client.subscribe("stat/ws63_sensor/SENSOR");
  setInterval(() => client.publish("cmnd/ws63_sensor/get", "1"), 5000);
});

client.on("message", (topic, payload) => {
  if (topic.endsWith("/SENSOR")) {
    const { temp, humid, press } = JSON.parse(payload);
    console.log(`T=${temp}°C H=${humid}% P=${press}hPa`);
  }
});
```

---

## 8. Home Assistant 接入（YAML）

```yaml
mqtt:
  sensor:
    - name: "WS63 Temperature"
      state_topic: "stat/ws63_sensor/temperature"
      unit_of_measurement: "°C"
      device_class: temperature
    - name: "WS63 Humidity"
      state_topic: "stat/ws63_sensor/humidity"
      unit_of_measurement: "%"
      device_class: humidity
    - name: "WS63 Pressure"
      state_topic: "stat/ws63_sensor/pressure"
      unit_of_measurement: "hPa"
      device_class: pressure

automation:
  - alias: "Poll WS63 every minute"
    trigger:
      - platform: time_pattern
        seconds: "/60"
    action:
      - service: mqtt.publish
        data:
          topic: "cmnd/ws63_sensor/get"
          payload: "1"
```

> HA 默认行为：MQTT sensor 在订阅后没有 retained 消息时会显示 `unknown`，**必须**搭配上面的 automation 主动 poll 才会有值。

---

## 9. 故障排查

| 现象 | 原因 | 处理 |
|---|---|---|
| 发了 `cmnd/get` 收不到 `stat/...` | 设备离线 / WiFi 断 / broker 拒绝 | 串口日志看 `[mqtt] connected ...` 是否打印；`[mqtt] connection lost` 表示掉线在重连 |
| 收到一组响应后再发请求没反应 | 客户端 Client ID 撞了设备的 `ws63-sensor-a1` | 换一个独一无二的 Client ID |
| 订阅 `stat/...` 一直静默 | 没人发 `cmnd/get`，本设计就是这样 | 自己定时 publish 或在 UI 上加"刷新"按钮 |
| `press` 一直在 1013.2 附近不动 | 正常 —— 大气压本来就稳定，分辨率 0.1 hPa | 关注趋势而非绝对值 |
| `temp` 比真实环境高 1–2 °C | PCB 自发热影响 SHT30 | 把传感器拉到板外、远离 WS63 / TM1640 |
| JSON 解析失败 | 主题混了，把单值主题当 JSON 解 | 只 parse `.../SENSOR`，单值主题用 `float()` |

### 验证设备是否真的在线

订阅 `$SYS/broker/clients/connected` 看不到设备级粒度。最直接的办法：

```bash
# 同时订阅 stat 和发 cmnd
mosquitto_sub -h 121.41.23.138 -u public -P Aa123456 \
              -t 'stat/ws63_sensor/SENSOR' &
mosquitto_pub -h 121.41.23.138 -u public -P Aa123456 \
              -t 'cmnd/ws63_sensor/get' -m 1
```

200 ms 内有响应 → 在线；超过 1 s 无响应 → 设备不在线或 broker 链路有问题。

---

## 10. 后续可能扩展（当前固件未实现）

如果业务需要，固件层可后续加上：

- **LWT（遗嘱）**：设备掉线时 broker 自动推送 `tele/ws63_sensor/LWT = "Offline"`，客户端无须 poll 即可感知离线。
- **批量响应抑制**：只发 `stat/.../SENSOR`，不再发三个单值主题，减少流量。
- **采样参数可调**：通过 `cmnd/ws63_sensor/config` 接收 JSON 改采样率 / 启停 BMP280。
- **历史缓冲**：本地 RAM 缓存最近 N 条，`cmnd/get` 携带参数 `{"history":10}` 返回数组。

如需添加请提 issue / 在分支上直接改。
