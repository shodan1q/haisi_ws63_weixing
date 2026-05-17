# haisi_ws63_weixing

WS63 SDK 个人开发分仓 —— 目前内容是 **4T_HRM_QS2 开发板的 LCD demo**
（ILI9341 2.8 寸屏，4 线软件 SPI 驱动）。

挂载点 `src/application/samples/custom/`，**SDK 源码零修改**。

---

## 怎么用（Windows）

### 第一次设置

1. 准备好 WS63 SDK 母体到 `D:\fbb_ws63-master\`。

2. PowerShell 把本仓库 clone 进 SDK 扩展点目录，**末尾的 `custom` 是重命名**：

   ```powershell
   cd D:\fbb_ws63-master\src\application\samples
   git clone https://github.com/shodan1q/haisi_ws63_weixing.git custom
   ```

3. 如果之前已经 clone 过旧版（占位 `main.c`），更新即可：

   ```powershell
   cd D:\fbb_ws63-master\src\application\samples\custom
   git pull
   ```

4. HiSpark Studio 工程根选 `D:\fbb_ws63-master\src`。

5. **Kconfig → Application → ✓ Enable Sample**（主开关 `SAMPLE_ENABLE`），保存。
   不要勾任何 sub-sample（如 helloworld），否则会和我们的 `app_run` 同时跑。

6. **Clean + Build**（一定要 clean，因为新增了文件，需要 cmake 重新扫）。

7. 烧录，按 RST 复位。

### 预期效果

- 串口看到：`***** Software LCD Flash Test Start *****`
- 屏幕：先蓝色清屏，然后白底黑字显示 `Line 0` ~ `Line 9` 共 10 行

---

## 硬件引脚（来自 `lcd.c`）

| 信号 | GPIO | 接 |
|---|---|---|
| SPI_SCK | 6 | LCD CLK |
| SPI_MOSI | 1 | LCD DI |
| SPI_MISO | 4 | LCD DO |
| LCD_CS | 5 | LCD CS |
| LCD_WR (D/C) | 3 | LCD WR/DC |
| FLASH_CS | 14 | Flash CS (本 demo 不用，仅初始化为高) |
| SD_CS | 2 | SD CS (本 demo 不用) |
| RFID_CS | 13 | RFID CS (本 demo 不用) |

引脚是 4T_HRM_QS2 板的硬连接，换板需要改 `lcd.c` 顶部的宏。

---

## 文件清单

| 文件 | 说明 |
|---|---|
| `lcd_demo.c` | 入口：创建任务初始化 LCD 并打印 10 行文字。`app_run(base_lcd_demo)` 自动注册 |
| `lcd.c` / `lcd.h` | ILI9341 4 线软件 SPI 驱动 |
| `fonts.c` / `fonts.h` | 字库（16x24 ASCII） |
| `CMakeLists.txt` | 把以上 3 个 .c 加进 samples 组件 |
| `.gitignore` / `.gitattributes` | 标配 |

源码来自 4T_HRM_QS2 开发板原厂 demo，已修复 `lcd_demo.c` 里
`#include "LCD.h"` 大小写问题（Windows 编译没事，Linux/Mac 会爆找不到文件）。

---

## 上游

- WS63 SDK 上游：https://gitee.com/HiSpark/fbb_ws63
- 完整 SDK 镜像：https://github.com/shodan1q/haisi_ws63
- 板厂资料：`/4T_HRM_QS2-main/` （HuaQing 4T_HRM_QS2 开发板）
