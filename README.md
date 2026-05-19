# haisi_ws63_weixing · PN532 NFC test (branch nfc-5321)

PN532 / PN5321 NFC 模块 over **UART1**（GPIO_26 TX / GPIO_27 RX，115200 8N1）的读卡测试。
LCD 实时显示 PN532 状态、固件版本、最近一张卡的 UID、累计读卡数。

挂载点 `src/application/samples/custom/`，SDK 源码零修改。

---

## 接线

| PN532 引脚 | 接板子上 |
|---|---|
| **TXD** | GPIO_27 (UART1_RX) |
| **RXD** | GPIO_26 (UART1_TX) |
| **VCC** | 3.3V 或 5V（看你模块是否支持 5V）|
| **GND** | GND |

⚠️ **必须**：PN532 模块上有 `SET0/SET1` 跳线/拨码，**全置 0 = UART 模式**。如果默认是 I2C 或 SPI 不会响应。

板上 UART0（USB 转串口）保持原样，HiSpark Studio 串口监视器仍能看到 `[nfc]` / `[pn532]` 调试日志。

---

## 切到这个分支编

```powershell
cd D:\fbb_ws63-master\src\application\samples\custom

# 切到 NFC 分支（如果第一次拉这个分支）
git fetch
git checkout -b nfc-5321 origin/nfc-5321
# 或者已经在本地：
# git checkout nfc-5321 && git pull

# HiSpark Studio: Clean → Build → 烧录 → 复位
```

---

## 预期行为

复位后约 1 秒：

**LCD**:
```
WS63 PN532 NFC          (绿色标题)
UART1 GPIO_26/27
                        
PN532: READY            (绿)
FW: 32 v1.6             (PN532 通常返回 IC=0x32 v1.6)
UID: 04 A1 B2 C3 ...    (刷一张卡这一行就显示其 UID)
Hits: 0, 1, 2...        (每识别一张新卡 +1)
Uptime: N s
```

**串口** (UART0 / USB)：
```
[pn532] SAMConfiguration ok
[nfc] firmware: IC=32 Ver=1 Rev=6 Support=07
[nfc] card UID (4 bytes): 04 A1 B2 C3
```

---

## 排错

| 现象 | 含义 |
|---|---|
| LCD `PN532 INIT FAILED` 红 | UART 没握上手。检查 RX/TX 是不是接反了、PN532 跳线模式、电源 |
| LCD `PN532: READY` 但 UID 永远 `(none)` | 通讯正常但探测不到卡。模块天线没接好 / 卡型号不支持（PN532 主要支持 ISO14443-A，Mifare/NTAG）|
| LCD `PN532 INIT FAILED` + 串口看到 `pn532_init failed` 但没有 SAMConfiguration log | UART 初始化都没成功，可能 GPIO mode 选错 |
| 串口里 `SAMConfiguration failed rc=-101` | 收到了响应但不是合法 ACK，可能波特率不对 |

`-rc` 错误码解读（在 `nfc/pn532.c`）：
- `-100`：发送失败
- `-101`：ACK 等待超时或不匹配（最常见的"接线问题"信号）
- `-1..-9`：响应帧解析各步骤错误

---

## API

`nfc/pn532.h` 公开：

```c
int pn532_init(void);
int pn532_get_firmware_version(uint8_t out[4]);

int pn532_read_card_uid(uint8_t uid[10], uint8_t *uid_len);

int pn532_mifare_read_block(uint8_t key_type, const uint8_t key[6],
                            const uint8_t *uid, uint8_t uid_len,
                            uint8_t block, uint8_t out[16]);

int pn532_mifare_write_block(uint8_t key_type, const uint8_t key[6],
                             const uint8_t *uid, uint8_t uid_len,
                             uint8_t block, const uint8_t data[16]);
```

写卡示例（写 block 4 = sector 1 block 0）：
```c
uint8_t key_a[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   // 出厂默认 KeyA
uint8_t payload[16] = {'H', 'i', '!', 0};                  // 16 字节，剩余补 0
pn532_mifare_write_block(0x60, key_a, uid, uid_len, 4, payload);
```

读回：
```c
uint8_t buf[16];
pn532_mifare_read_block(0x60, key_a, uid, uid_len, 4, buf);
// buf 现在是 'H' 'i' '!' 0x00 ... ...
```

⚠️ **不要写 sector trailer**（block 3、7、11、…即 `(n*4)+3`）除非你确定 Key A/B + access bits 格式，否则该 sector 永久变砖。

---

## 分支总览

| 分支 | 内容 |
|---|---|
| `main` | SG90 舵机平滑扫描 demo（GPIO_10） |
| `sle-speed-backup` | 两板 SLE 通信（sle_speed_server + client） |
| **`nfc-5321`** | 当前分支：PN532 NFC 读卡 |

---

## 文件清单（本分支）

| 文件 | 说明 |
|---|---|
| `app_demo.c` | 主入口，启动 LCD + NFC 任务 |
| `nfc/pn532.c` | PN532 UART 驱动 + 命令封装 |
| `nfc/pn532.h` | 公开 API |
| `lcd.c` / `lcd.h` / `fonts.c` / `fonts.h` | ILI9341 LCD 驱动 |
| `CMakeLists.txt` | 编译清单 |

---

## 上游

- WS63 SDK 上游：https://gitee.com/HiSpark/fbb_ws63
- 完整 SDK 镜像：https://github.com/shodan1q/haisi_ws63
- PN532 协议参考：NXP UM0701-02 (PN532 User Manual)
