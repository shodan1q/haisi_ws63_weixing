# WS63 NearLink Provisioning · HarmonyOS App

A 3-file HarmonyOS (ArkTS) app that scans for the WS63 board's SLE server,
connects, and writes Wi-Fi credentials into it.

This pairs with the board firmware in the parent directory — the SLE
server in `../sle_server/` and the new write-callback in
`../sle_server/src/SLE_Distribute_Network_Server.c` that turns the incoming
117-byte payload into a Wi-Fi connect.

---

## 把代码放到 DevEco Studio 工程里

假设你的工程根目录结构是默认的：

```
your_app/
├── entry/
│   └── src/main/
│       ├── ets/
│       │   ├── entryability/
│       │   └── pages/
│       │       └── Index.ets        ← 替换成本目录里的
│       └── module.json5             ← 加权限（见下面）
```

复制 / 粘贴：

| 仓库里的文件 | 放到工程里的位置 |
|---|---|
| `Index.ets` | `entry/src/main/ets/pages/Index.ets` |
| `NearLinkService.ets` | `entry/src/main/ets/common/NearLinkService.ets` |
| `ProvisioningPayload.ets` | `entry/src/main/ets/common/ProvisioningPayload.ets` |

如果你工程的目录名不一样，调整 `Index.ets` 顶部那两行 `import` 的相对路径。

---

## module.json5 加权限

在 `entry/src/main/module.json5` 的 `module` 节点里：

```json5
{
  "module": {
    "...": "...",
    "requestPermissions": [
      { "name": "ohos.permission.ACCESS_NEARLINK" },
      { "name": "ohos.permission.APPROXIMATELY_LOCATION" }
    ]
  }
}
```

- `ACCESS_NEARLINK` — NearLink 扫描 / 连接的基础权限
- `APPROXIMATELY_LOCATION` — 多数蓝牙 / 短距通信扫描接口要求位置权限（隐私法规要求）

在 UIAbility 的 `onWindowStageCreate` 里调一次动态权限申请（鸿蒙运行时权限模型），否则用户拒绝时 App 会静默失败。最简版本：

```typescript
import abilityAccessCtrl from '@ohos.abilityAccessCtrl';

const atManager = abilityAccessCtrl.createAtManager();
await atManager.requestPermissionsFromUser(this.context, [
  'ohos.permission.ACCESS_NEARLINK',
  'ohos.permission.APPROXIMATELY_LOCATION',
]);
```

---

## NearLink Kit API 可能要你调整

`NearLinkService.ets` 顶部用的是 **`@kit.NearLinkKit`**，这是 HarmonyOS NEXT (API 12+) 的标准 NearLink Kit 入口。如果你的 DevEco Studio 里：

- 没有 `@kit.NearLinkKit` → 试 `@ohos.nearlink` 或者从 `@hms.nearlink.*` 引入
- 子模块名 `access` / `scan` / `connection` / `ssap` 可能略有差别（例如某些版本里叫 `ble` 或者全部并到一个 `nearlink` 命名空间下）
- 事件名（`'deviceFound'`、`'connectionStateChange'`、`'servicesDiscovered'`）以你 SDK 的 IDE 提示为准

`Index.ets` 和 `ProvisioningPayload.ets` 不依赖具体 API，只 `NearLinkService.ets` 这一层会被影响——所以只需要按你 IDE 的真实接口签名调整这一个文件，其它两个不动。

---

## 操作流程

1. 装 App 到鸿蒙手机
2. 上电 / 复位板子，等 LCD 显示 `SLE: ADVERTISING`
3. 打开 App → 点 `Scan for SLE_DISTRIBUTE_SERVER`
4. 列表里看到 `SLE_DISTRIBUTE_SERVER` → 点它
5. 状态变成 `ready` 后填 Wi-Fi SSID 和密码
6. 点 `Send Credentials`
7. 板子串口会打印 `[prov] creds stored: ssid=xxx`，LCD 第 7 行显示 `SSID: xxx` (绿)
8. WiFi 任务开始连接，LCD 第 6 行从 `WiFi: waiting creds` 变成 `WiFi: connecting...` → `WiFi: CONNECTED`

---

## 字节协议（如果你想自己写客户端）

写入 Property UUID `0x3344`（完整 UUID `37BEA880-FC70-11EA-B720-0000-33440000`）的内容是 **117 字节固定结构**：

| 偏移 | 字节数 | 内容 |
|---|---|---|
| 0 | 17 | flag：`"WIFI_SSID_KEY\0"` 后面补 0 |
| 17 | 1 | ssid_len：SSID 字节数 + 1（含 `\0`） |
| 18 | 1 | key_len：密码字节数 + 1（含 `\0`） |
| 19 | 33 | ssid：`\0` 结尾，剩余补 0 |
| 52 | 65 | key：`\0` 结尾，剩余补 0 |

板端收到这个 117 字节后会校验 flag，匹配则提取 SSID / 密码并触发 `wifi_sta` 连接。其它长度的 write 落到旧的"返回硬编码凭据"分支（HiHope 原 sample 的两板演示模式）。

---

## 如何确认整个链路通了

1. **板子 LCD 第 7 行出现 `SSID: <你输入的>` （绿）** → SLE 写入成功，板端解析正确
2. **板子串口出现** `[prov] creds stored: ssid=...` → 同上
3. **板子 LCD WiFi 行从 `waiting creds` 变 `CONNECTED`** → 完整闭环
4. **板子 ping 得通** → 大功告成
