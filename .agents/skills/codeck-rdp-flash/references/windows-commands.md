# 本项目 Windows 命令和串口限制

在项目根目录，通过独立 PowerShell 进程运行脚本，避免构建环境设置影响其他任务：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/codeck-rdp-flash/scripts/Build-Firmware.ps1 -DryRun
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/codeck-rdp-flash/scripts/Build-Firmware.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/codeck-rdp-flash/scripts/Invoke-RdpFlash.ps1 -Port COM5 -Mode full -DryRun
```

上述准备完成后提示用户进入下载模式；收到“已进入下载模式”再执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/codeck-rdp-flash/scripts/Invoke-RdpFlash.ps1 -Port COM5 -Mode full
```

已确认设备的引导和分区未变时，将两条烧录命令的 `-Mode full` 同时改为 `-Mode app`。脚本只执行一次，不负责构建新鲜度、物理按键确认或启动验收。不要在前一个刷机/监视进程还占用端口时启动另一个。

## 当前工具与布局

构建脚本使用当前已安装的 ESP-IDF 6.1、Python 虚拟环境、CMake、Ninja 和 Xtensa 编译器；迁移机器时先核实安装路径再调整脚本，不能把路径不存在误报成串口故障。

当前 `build/flasher_args.json` 指定 DIO / 80 MHz / 16 MB：bootloader 在 `0x0`，分区表在 `0x8000`，应用在 `0x10000`。`partitions.csv` 中 NVS 是 `0x9000` 起、`0x6000` 长，factory 应用为 2 MiB。构建输出的容量检查是主要依据。不得写 NVS 范围或整片擦除；若准备修改布局，先评估已有数据，影响旧 NVS 的操作需另行明确授权。

脚本动态读取构建元数据，并对已知布局做保护。不要手抄旧版本应用大小。若直接使用生成的 `flash_args`，PowerShell 中必须引用 `'@flash_args'`，并从 build 目录运行。

## 需要采集启动日志时

使用 ESP-IDF 虚拟环境里的 pyserial，115200 波特率。监视器独占串口，烧录前必须关闭。RDP 上已验证的打开方式为先创建未打开的对象，然后设置：

```python
port = serial.Serial()
port.port = "COM5"
port.baudrate = 115200
port.timeout = 0
port.dtr = True
port.rts = False
port.open()
# 在有界循环中：
available = port.in_waiting
if available:
    data = port.read(available)
# finally 中 port.close()
```

不要以阻塞 `read(4096)` 采集：之前在 RDP 上出现 `GetOverlappedResult failed (OSError 38)`。在 30–60 秒的总时限内，捕获 SerialException/OSError 后关闭并短暂重试；设备 USB 重新枚举时重新确认端口。解析日志前先对凭据、密码和 Authorization 脱敏，只保存脱敏内容到 build 目录。监视准备好再请用户短按 RESET（不要按 BOOT），便于捕获启动日志；串口连接本身可能改变控制线状态，采集后仍需确认屏幕。

空日志可能是启动输出错过、仍在下载模式或 RDP 断流，不证明正常启动。重复 FT3168 触摸读错误曾出现，避免输出洪泛但应统计并报告；panic 和显示初始化错误需保留诊断信息。
