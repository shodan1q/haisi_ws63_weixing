# haisi_ws63_weixing · SG90 servo demo

WS63 个人开发分仓 —— 当前内容是 **SG90 舵机控制 demo**（GPIO_10 软件模拟 50Hz PWM）。

挂载点 `src/application/samples/custom/`，SDK 源码零修改。

---

## 当前 demo 做什么

启动后舵机循环以下动作（一圈约 3.2 秒）：
1. **居中 0°**  (1500 µs 脉宽)
2. **左转 +90°** (2500 µs 脉宽) 🔴
3. **居中 0°**
4. **右转 -90°** (500 µs 脉宽) 🔵

LCD 显示：
- 顶行 `WS63 SG90 servo`
- `Pin: GPIO_10 (PWM)`
- `ANGLE: ±90 / 0 (left/right/center)` 实时跟随舵机当前角度，颜色变化
- `Moves: N` 累计执行次数
- `Uptime: N s`

## 为什么是软件 PWM 不是硬件 PWM

舵机要求 **50 Hz（20 ms 周期）**，WS63 硬件 PWM 最低频率比这个高，达不到。所以照 HiHope 官方做法 **GPIO bit-bang**：用 `uapi_systick_delay_us` 拉高 N µs 再拉低 (20000-N) µs。

不优雅但有效，CPU 占用约 1%（每秒只发约 50 个 20ms 周期，发的时候才阻塞）。

---

## 接线

| SG90 引脚 | 接 | 说明 |
|---|---|---|
| **橙色（信号）** | 板上 GPIO_10 | 50Hz PWM 信号 |
| **红色（VCC）** | 板上 5V | 舵机供电（**注意：5V，不是 3.3V，否则没力**） |
| **棕色（GND）** | 板上 GND | 地 |

⚠️ **重要**：
- SG90 电流峰值 ~250mA，板载稳压能否给够要看你板子，最稳妥是用外部 5V 电源 + 共地
- GPIO_10 在 4T_HRM_QS2 板上原本接 LED1 红灯，如果板子焊死了 LED 在那一脚，你会看到 LED 随着舵机控制信号亮一下亮一下——属于正常副作用

---

## Windows 上怎么编

```powershell
cd D:\fbb_ws63-master\src\application\samples\custom
git pull
git log --oneline -1
# 应该是最新的 servo demo commit

# HiSpark Studio:
#   Kconfig → Application → ✓ Enable Sample → Save
#   Clean → Build → 烧录 → 复位
```

---

## 文件清单

| 文件 | 说明 |
|---|---|
| `app_demo.c` | 主入口，启动 LCD + servo 任务 |
| `servo/sg90_control.c` | 软件 PWM 实现（GPIO bit-bang） |
| `servo/sg90_control.h` | 暴露入口 + 全局状态 |
| `lcd.c` / `lcd.h` / `fonts.c` / `fonts.h` | ILI9341 LCD 驱动（板厂原版） |
| `CMakeLists.txt` | 编译清单 |

---

## 想改什么

| 想改 | 改这里 |
|---|---|
| 切换不同角度（如转 45°） | `sg90_control.c` 顶部 `US_FOR_*` 宏，1500 居中，每变 1µs 约 0.09° |
| 改成连续旋转 360° | SG90 是位置舵机，硬件不支持；要换成 MG996R 连续旋转版 |
| 减速 / 加速过渡 | 把 `servo_move_to` 改成发送一系列渐变的 high_us 值 |
| 不要循环，停在某个角度 | 在 `servo_task()` 的 `for(;;)` 里只 call 一次然后 sleep 长时间 |
| 用按键控制角度 | 之前 LCD+LED+ADC 按键的版本在 git history `f9266cd` commit |

---

## 历史代码

| 想找什么 | 在哪 |
|---|---|
| **两板 SLE 双向通信**（sle_speed_server + client） | 分支 `sle-speed-backup`，commit `8af5048` 之前的 main |
| LCD + LED + 蜂鸣器 + ADC 按键综合 demo | main 分支 commit `f9266cd` |
| SLE 配网 + HarmonyOS App | 分支 `sle-speed-backup` 之前的 `harmony_app/` 目录（或 commit `dad6991`） |
| LCD 单显示 demo | commit `163d756` |

切回任意旧版本（**不要**这么做除非你想改回去）：
```powershell
git checkout <commit-hash>      # 临时切去查看
git checkout main               # 切回主线
```

要把 sle-speed-backup 分支拉到本地：
```powershell
git fetch origin sle-speed-backup
git checkout sle-speed-backup
```

---

## 上游

- WS63 SDK 上游：https://gitee.com/HiSpark/fbb_ws63
- 完整 SDK 镜像：https://github.com/shodan1q/haisi_ws63
- SG90 demo 参考自：`vendor/HiHope_NearLink_DK_WS63E_V03/demo/servo/sg92r_control.c`
