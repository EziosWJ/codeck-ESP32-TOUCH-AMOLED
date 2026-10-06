---
name: codeck-host-screenshots
description: 在 Codeck 项目中编译并运行真实 LVGL 主机模拟测试，导出设备五个页面及关键状态的截图，检查图片并更新 README。适用于电脑预览、页面截图和界面回归；设备烧录与手机配网页面截图使用各自流程。
---

# Codeck 主机模拟与页面截图

从本技能所在项目根目录执行。用户只要求创建或解释技能时，仅处理技能；请求运行、
截图或更新文档时再执行相应流程。复用项目已有测试和导出脚本，不另建网页来冒充设备界面。

## 模拟方式与入口

- `tests/host/CMakeLists.txt` 构建主机 C 程序 `test_ui`，使用真实 LVGL，与固件共用
  `main/ui/ui_home.c`、`main/ui/ui_dashboard.c`、字体和 `main/input/lvgl_touch_input.c`。
- `tests/host/test_ui.c` 创建 **466 × 466** 虚拟显示屏，通过模拟指针输入完成滑动、
  点击和页面切换。它是离屏测试，不会弹出交互式桌面窗口，也不是完整 ESP32 模拟器。
- 网络、配网和硬件接口使用 `tests/host/stubs/` 的 SDK 替身，额度、时间、成本、
  任务数量等由测试注入。不要将这些截图描述成真实账户、网络请求或实机运行结果。
- `snapshot()` 更新布局后调用 `lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888)`，
  按绘图缓冲的 stride 逐行读取，将 LVGL 的 BGR 字节顺序转换成 RGB，写出 P6 PPM。
  新增截图沿用该函数，避免自行直接复制缓冲造成颜色或行对齐错误。
- [`export_ui_screenshots.py`](../../../tests/host/export_ui_screenshots.py) 用 Python
  标准库无损转换为 PNG，无需 Pillow 或 ImageMagick。原始文件在 `build/host/`，
  文档图片在 `docs/screenshots/`；当前导出清单为 26 张，以脚本中的 `SNAPSHOTS` 为准。

## 运行并导出

先检查项目规范和现有改动，保留用户代码。确认
`managed_components/lvgl__lvgl` 与 `managed_components/espressif__cjson/cJSON` 已存在；
缺少时在已激活的 ESP-IDF 终端运行 `idf.py build` 下载依赖，不需要烧录设备。
主机构建使用 Linux / WSL 的 C 编译器、CMake 和构建工具；导出需要 Python 3。

在项目根目录的 Linux / WSL 终端执行，逐步检查退出码：

```sh
cmake -S tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host -j 8
ctest --test-dir build/host --output-on-failure
python3 tests/host/export_ui_screenshots.py
```

使用 Debug 构建以保留测试断言。CTest 的 `ui` 测试会在 `build/host` 写入截图；
`ui_device_heap` 设置 `CODECK_UI_HEAP_BYTES=131072`，仅验证设备堆预算并跳过截图。
父进程不要携带 `CODECK_UI_HEAP_BYTES` 运行普通截图测试。
若只需要刷新界面截图，重新构建后可执行：

```sh
(cd build/host && env -u CODECK_UI_HEAP_BYTES ./test_ui)
python3 tests/host/export_ui_screenshots.py
```

`test_ui` 的 PPM 路径相对当前工作目录，必须从 `build/host` 运行，不能从项目根目录
直接运行后误用旧快照。构建或测试失败时先处理失败，不把原有 PPM/PNG 当作新结果。
运行前后核对截图修改时间，确认本次普通 UI 测试确实生成了目标文件。

### Windows / WSL

先 `wsl --list --quiet` 查看发行版，并确认所选环境有 `cmake` 和 `cc`。
2026-10-06 本机验证可用的发行版名为 **ext4**；名称是环境记录，不是固定依赖。
以下 PowerShell 命令从项目根目录执行，其他机器替换发行版名称：

```powershell
wsl -d ext4 -- cmake -S tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
wsl -d ext4 -- cmake --build build/host -j 8
wsl -d ext4 -- ctest --test-dir build/host --output-on-failure
python tests/host/export_ui_screenshots.py
```

Windows 的 Python 和 WSL 的 Python 都可导出同一批 PPM。
本机 Windows Python 曾位于 `D:/Espressif/tools/python/python.exe`，优先使用当前环境
可用的 `python`。若项目移动或 CMake 缓存的源码路径不符，改用新的构建目录，并给
导出脚本传入 `--source <构建目录>`，无需删除已有固件构建目录。

## 页面与状态覆盖

| 设备页面 | README 主图 |
| --- | --- |
| 首页 | `ui_home_reference.png` |
| 额度详情 | `ui_quota_details.png` |
| 成本统计 | `ui_cost.png` |
| 联网状态 | `ui_network_online.png` |
| 网络设置 | `ui_settings_idle.png` |

页面顺序为首页 → 额度详情 → 成本统计 → 联网状态 → 网络设置，两端不循环。
完整状态截图还包含：首页等待、95%/100%/0%；周额度缺失、额度边界和时间未同步；
成本等待、上涨、非法值和零值；断网与未配置；配网确认、等待、首次启动、连接中、
API 401、关闭热点、连接超时及启动失败。

新页面或新状态在 `test_ui.c` 中更新对应 view、模拟切换，并在确认目标页已激活后
调用 `snapshot()`。同步更新导出脚本的 `SNAPSHOTS` 和文档引用；截图数量会随覆盖变化，
不要只复制旧图片或硬编码历史数量作为成功依据。

## 检查与 README 更新

1. 记录实际构建和测试结果。此前环境无 Node.js 时为 7 项 CTest；存在 Node.js 时会
   增加 `provision_web`。报告本次实际数量，不声称未运行的可选测试通过。
2. 通过图片查看工具逐张检查中文字体、文字裁切、额度环、图表、按钮和弹窗。
   图片较多时可在 `build/host` 制作临时拼图审阅，保留原始 PNG 作为交付图。
3. 确认 PNG 为 466 × 466、链接目标存在、覆盖本次要求的全部页面和状态。
   若怀疑转换异常，检查 PNG CRC，并将解压后的 RGB 像素与 PPM 正文逐字节比较。
4. 用户要求更新文档时，更新 [`README.md`](../../../README.md) 的“电脑模拟截图”部分。
   使用 `docs/screenshots/` 的仓库相对路径；不要引用被忽略的 `build/` 或本机绝对路径。
   主页面直接展示，额外状态可放在折叠区域。补充当前运行命令、截图日期和样例数据说明。
5. 标明成本、任务、运行数量目前未映射到账户接口，实际缺失数据显示占位符。
   `Codeck-1234` 与 `a1b2c3d4e5f6` 是测试热点与密码，截图时间为固定样例值。
   主机截图不证明真实 Wi-Fi、手机配网页面、HTTPS、屏幕驱动或触摸手感已通过验证。
6. 完成 `git diff --check` 并检查改动范围。交付 README、截图目录及实际测试结果；
   未请求时不提交或推送代码。

## 常见问题

- **沙箱初始化失败**：本机曾出现 `helper_unknown_error: setup refresh had errors`，
  PowerShell 和 Node REPL 均无法启动。这发生在执行前，不是编译错误；按工具权限机制
  重试必要命令，不能把“程序未运行”报告为“测试失败”或“测试通过”。
- **WSL 挂载失败**：本机 `Ubuntu24.04` 曾报告 `MountDisk/HCS/E_ACCESSDENIED`；
  `Ubuntu-26.04` 能运行旧二进制但缺少编译工具，`ext4` 能完成重新构建。
  检查其他已安装环境，选有工具的环境；不要仅运行旧二进制就声称当前源码已验证。
  不为截图任务擅自重装发行版、修改磁盘权限或关闭系统服务。
- **转换提示缺少 PPM**：检查 UI 测试是否成功、工作目录是否正确、是否设置了
  `CODECK_UI_HEAP_BYTES`，以及新增状态是否已与导出清单同步。
- **中文输出乱码**：PowerShell 读取文档使用 `Get-Content -Encoding UTF8`；
  避免根据终端错误解码重写原本正确的 README。
