# haisi_ws63_weixing

WS63 SDK 上的个人开发分仓。只跟踪自己的应用代码，不带 SDK 母体；
克隆到 SDK 的预留扩展点 `src/application/samples/custom/`，**SDK 源码零修改**。

---

## 怎么用（Windows）

### 第一次设置

1. 先准备好 WS63 SDK 母体（任何方式：HiSpark Studio 自动下载、Gitee `HiSpark/fbb_ws63` ZIP、U 盘拷贝……），假设解压到 `D:\fbb_ws63-master\`。

2. 在 PowerShell 把本仓库 clone 进 SDK 的扩展点目录。**注意末尾的 `custom`** —— 这是把仓库下载下来后重命名成 SDK 期望的目录名：

   ```powershell
   cd D:\fbb_ws63-master\src\application\samples
   git clone https://github.com/shodan1q/haisi_ws63_weixing.git custom
   ```

   完成后目录是这样：
   ```
   src/application/samples/custom/
   ├── .git/
   ├── .gitignore
   ├── .gitattributes
   ├── README.md
   ├── CMakeLists.txt
   └── main.c
   ```

3. 打开 HiSpark Studio，工程根选 `D:\fbb_ws63-master\src`，新建工程。

4. **Kconfig → Application → ✓ Enable Sample**（主开关 `SAMPLE_ENABLE` 打开就行，不需要勾任何 sub-sample），保存。

5. `Rebuild` → 烧录 → 串口监视器，能看到：
   ```
   [weixing] task started
   [weixing] alive
   [weixing] alive
   ...
   ```

### 日常开发

只在 `custom/` 这个目录里改代码，git 只管这个仓库：

```powershell
cd D:\fbb_ws63-master\src\application\samples\custom
git status                # 只会显示你自己的改动
git add -A
git commit -m "your message"
git push
```

SDK 编译会自动把 `main.c` 编进 samples 组件并链接进固件，
入口函数通过 `app_run(weixing_entry)` 宏在 boot 时自动调用。

---

## 为什么这么放

`src/application/samples/CMakeLists.txt` 里有这一行：

```cmake
add_subdirectory_if_exist(custom)
```

——SDK 官方预留的"用户自定义子目录"挂载点，`_if_exist` 表示文件夹不存在也不报错。
所以这个仓库 clone 进去就能编，不 clone 也不会有问题。

`samples` 这个组件本身在 `SAMPLE_ENABLE=y` 时被链接进最终固件，
因此 Kconfig 里把这个主开关打开就够了，不需要也没有 sub-sample 的开关。

---

## 改成多个应用

随着开发推进，如果你想在这一个仓库里放多个 app，把目录结构改成：

```
custom/
├── CMakeLists.txt         ← add_subdirectory_if_exist(app_a) + add_subdirectory_if_exist(app_b)
├── app_a/
│   ├── CMakeLists.txt     ← set(SOURCES ... PARENT_SCOPE)
│   └── main.c
└── app_b/
    ├── CMakeLists.txt
    └── main.c
```

---

## 文件清单

| 文件 | 说明 |
|---|---|
| `main.c` | 入口示例，使用 osal 创建一个循环 printk 的任务，`app_run` 宏自动注册 |
| `CMakeLists.txt` | 把 `main.c` append 到 samples 组件的 SOURCES（标准约定） |
| `.gitignore` | 排除编译中间产物 |
| `.gitattributes` | Windows 开发场景下的行尾归一化 |

---

## 上游 SDK

- 上游：https://gitee.com/HiSpark/fbb_ws63
- 完整 SDK 镜像（含 .gitignore/.gitattributes）：https://github.com/shodan1q/haisi_ws63
