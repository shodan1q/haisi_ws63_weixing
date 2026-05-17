# haisi_ws63_weixing

WS63 个人开发分仓 —— **NearLink (SLE) 配网 + WiFi STA + LCD 状态显示**。

挂载点：`src/application/samples/custom/`，SDK 源码零修改。

---

## 这个 demo 干什么

启动后，板子并行做两件事：
1. **SLE Server**：广播自己叫 `SLE_DISTRIBUTE_SERVER`，等鸿蒙手机扫描并配对/连接；
2. **WiFi STA**：用代码里硬编码的 SSID/密码连一个 WiFi AP（你要先在 `app_demo.c` 里改成自家的）。

LCD 显示：
```
WS63 NearLink Cfg                 (顶行 绿色)
SLE: SLE_DISTRIBUTE_SERVER        (这个是广播名)
  scan with HarmonyOS
WiFi: CONNECTED / FAILED / ...
Uptime: N s
```

> ⚠️ 当前是 **"板子用硬编码 WiFi + SLE 演示"**，不是真正的"手机经 SLE 把凭据下发给板子"。如果要做后者，看下面【真正的手机配网】章节。

---

## Windows 上怎么用

```powershell
cd D:\fbb_ws63-master\src\application\samples\custom
git pull

# 改 WiFi 凭据：编辑 D:\fbb_ws63-master\src\application\samples\custom\app_demo.c
#   第 28~29 行：
#     #define WIFI_SSID  "your_ssid_here"
#     #define WIFI_KEY   "your_password_here"

# HiSpark Studio:
#   Kconfig → Application → ✓ Enable Sample → Save
#   Clean → Build → 烧录 → 按 RST
```

---

## 鸿蒙手机端怎么连

1. 手机：设置 → 更多连接 → **NearLink/星闪** → 打开（HarmonyOS 4 及以上原生支持）
2. 在 NearLink 设备列表里应该能看到 `SLE_DISTRIBUTE_SERVER`
3. 点连接，板子串口会打印连接事件，LCD 没专门展示连接态（可自己扩展）

SLE 服务 UUID：
- Service UUID：`0xABCD`
- Property UUID：`0x3344`（可读+可写+notify）
- 完整 16 字节 UUID 基底：`37BEA880-FC70-11EA-B720-0000-00000000`（替换最后 2 字节为上面的 short ID）

这些 UUID 是 HiHope 原样保留，你写 HarmonyOS App 时用这些做 `ble.startScanForDevices` / `connect` / `write/notify` 参考。

---

## 配套的鸿蒙 App

仓库里 `harmony_app/` 目录提供了一个 3 文件 ArkTS App，**已经能跑通真·手机配网流程**：
扫描 → 连接板子 → 输入 SSID/密码 → 写入 → 板子收到并连 WiFi。

把 `harmony_app/{Index.ets, NearLinkService.ets, ProvisioningPayload.ets}` 拷到你 DevEco Studio 工程对应位置，并在 `module.json5` 加 NearLink 权限即可。详情看 `harmony_app/README.md`。

板端已经实现凭据接收：`example_network_info_write_request_cbk` 现在能识别 117 字节的标准结构并交给 `wifi_task` 去连。

---

## 旧的两板演示模式（保留）

当前 `sle_server/src/SLE_Distribute_Network_Server.c` 里的
`example_network_info_write_request_cbk()` 行为是：

> **手机写入 `"WIFI_SSID_KEY"` 这 13 字节字符串 → 板子用 notify 把硬编码的 SSID+key 发回手机**

这是"板子告诉手机凭据"的方向，给两板演示用的。

要改成"手机告诉板子凭据"，你需要：

1. 修改 `example_network_info_write_request_cbk()`：
   - 判断 `write_cb_para->length == sizeof(example_wifi_ssid_key_ntf_ind_t)`
   - 把 `write_cb_para->value` 强转成 `example_wifi_ssid_key_ntf_ind_t *`
   - 把里面的 `ssid` 和 `key` 提出来
   - 调用 `example_sta_function(ssid, ssid_len, key, key_len)` 启动 WiFi 连接
   - （注意：write 回调是 SLE 协议栈线程，不要阻塞太久；推荐丢到一个队列让 wifi_task 去做）

2. 手机端 App 发送的字节布局（结构体 `example_wifi_ssid_key_ntf_ind_t` 在 `Server.c` 第 68 行）：

   | 偏移 | 字节数 | 内容 |
   |---|---|---|
   | 0 | 17 | flag，建议填 "WIFI_SSID_KEY\0" |
   | 17 | 1 | ssid_len（含末尾 `\0`） |
   | 18 | 1 | key_len（含末尾 `\0`） |
   | 19 | 33 | SSID（`\0` 结尾） |
   | 52 | 65 | key（`\0` 结尾） |
   | **共 117 字节** |

3. App 走 `write_without_response` 写到 Property UUID `0x3344` 即可。

---

## 文件清单

| 文件 | 作用 |
|---|---|
| `app_demo.c` | 主入口，启动 LCD/SLE/WiFi 三块。WiFi SSID/key 在这里改 |
| `wifi_sta.c` | `example_sta_function(ssid, key)`：扫描→连接→DHCP→打印 IP。HiHope 原样 |
| `lcd.c` / `lcd.h` / `fonts.c` / `fonts.h` | ILI9341 LCD 驱动（板厂原版） |
| `sle_server/src/SLE_Distribute_Network_Server.c` | SLE GATT-like server（HiHope 原版，去掉 `app_run` 后改为 `sle_provisioning_server_start()` 由我们调用） |
| `sle_server/src/SLE_Distribute_Network_Server_adv.c` | SLE 广播参数 |
| `sle_server/inc/*.h` | UUID 定义 + 入口函数原型 |
| `CMakeLists.txt` | 把上面所有 .c 接进 samples 组件 |

---

## Kconfig 要打开什么

HiSpark Studio → Kconfig：
- **Application → ✓ Enable Sample**（必需，否则 samples 组件不进固件）

WiFi 和 SLE 的协议栈本身是 WS63 默认编进固件的，不需要额外开关。如果编译报 `wifi_sta_enable undefined` 或 `enable_sle undefined`，那是 SDK 默认配置问题，再来调。

---

## 上游

- WS63 SDK 上游：https://gitee.com/HiSpark/fbb_ws63
- 完整 SDK 镜像：https://github.com/shodan1q/haisi_ws63
- SLE 原 sample 来自：`vendor/HiHope_NearLink_DK_WS63E_V03/demo/sle_distribute_network/`
- WiFi STA 原 sample 来自：同上 `sle_distribute_network_client/src/WiFi_STA.c`

之前版本（LCD+LED+蜂鸣器+ADC 按键 demo）保留在 git history 里，commit `f9266cd` 之前都是。
