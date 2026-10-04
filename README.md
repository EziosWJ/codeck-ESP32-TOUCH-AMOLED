# Codeck 额度显示

ESP32-S3 / ESP-IDF 6.1 / LVGL 9.4。启动后连接 Wi-Fi，用 HTTP Basic Auth
获取 `/api/account`，显示 Codex 主额度窗口，默认每 60 秒刷新。

## 本地配置

复制 `main/config.local.example.h` 为 `main/config.local.h`，填写：

```c
#define CODECK_WIFI_SSID "你的 2.4 GHz Wi-Fi 名称"
#define CODECK_WIFI_PASSWORD "Wi-Fi 密码"
#define CODECK_API_URL "http://codeck.wangj.de/api/account"
#define CODECK_API_USERNAME "你的 API 用户名"
#define CODECK_API_PASSWORD "你的 API 密码"
```

`config.local.h` 已加入 Git 忽略。未填写 Wi-Fi 或认证信息时，固件保留可操作的
占位界面并在串口提示配置。首次构建使用组件管理器下载 `espressif/cjson`。
API 地址须可从设备所连网络访问；设备上的 `localhost` 指设备自身。

在已激活的 ESP-IDF 6.1 终端运行：

```powershell
idf.py build
idf.py -p COM3 flash monitor
```

完整网络固件约 1.39 MiB，使用 `partitions.csv` 的 2 MiB 应用分区。
已有 `sdkconfig` 时，在 `idf.py menuconfig` 的 Partition Table 中选择 Custom
partition table CSV，文件设为 `partitions.csv`，再构建。NVS、PHY 和应用的
起始地址与旧分区一致；首次升级需使用 `idf.py flash` 一并烧录新分区表。
已有配置还需启用 `CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH`，并将
`CONFIG_CJSON_NESTING_LIMIT` 设为 32；新项目由 `sdkconfig.defaults` 默认启用。

## 数据和状态

- 默认显示额度页，左滑进入联网状态页，右滑返回；底部圆点标识当前页面。
  全屏横滑超过 28 像素即时切换，支持慢速和略带斜向的滑动；一次触摸只切换一次。
  联网页底部可点击“< 额度”返回。触摸每 10 ms 采样，并过滤短暂丢点。
  页面切换只重绘目标页，
  额度圆环和文字只在内容变化时更新。
- 联网页每秒更新 Wi-Fi 名称、连接状态、IPv4 地址和信号强度（dBm），并显示
  额度服务、时间同步状态。Wi-Fi 连接独立于额度接口请求结果；断线时清空 IP
  和信号信息，保留配置的 Wi-Fi 名称。

- 使用 HTTP GET、`Codeck/1.0` User-Agent、10 秒传输超时，禁用自动重定向和认证重试。
- 要求 HTTP 200、`ok: true`；解析 `rate_limits.rateLimits.primary` 的
  `usedPercent`、`windowDurationMins`、`resetsAt`，忽略周额度及账户个人信息。
- 显示剩余百分比，300 分钟显示 `5H`；JSON 正文上限为 64 KiB，支持分块接收。
- 获取 IP 后立即请求，随后每 60 秒刷新；Wi-Fi 断线后以至少 5 秒间隔重连。
- SNTP 使用 `ntp.aliyun.com`，按北京时间显示时钟和重置时间，每秒更新倒计时。
  同步前隐藏时间；倒计时到零时等待服务器数据，不自动恢复额度。
- 状态灯绿色表示最近一次请求成功且 Wi-Fi 已连接；橙色表示断网或请求失败。
  请求失败保留最后有效额度；首次请求前显示占位符。
- 日志只记录请求状态和剩余百分比，不输出认证凭据、个人信息或响应正文。

## 验证

在 Linux 或 WSL 中，完成一次 ESP-IDF 构建以下载依赖后运行：

```sh
cmake -S tests/host -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host -j 8
ctest --test-dir build/host --output-on-failure
```

测试覆盖额度映射、非法/缺失字段、分块及 64 KiB 上限、北京时间和倒计时、
认证配置、401/重定向/超时、错误数据保留、重连及刷新调度。
网络任务使用 SDK 替身；界面测试使用真实 LVGL，生成 `build/host/ui_*.ppm`，
并验证左右触摸滑动、联网/断线/未配置显示、接口失败与 Wi-Fi 状态独立，
以及 1000 次更新不重新创建控件。

实机需另外验证：首次联网、两次间隔刷新的数据、拔网后的状态灯及重连恢复、
SNTP 时间同步，以及网络请求期间触摸响应。字体字符变化后可在 Windows 执行
`main/ui/font/generate_ui_home_font.ps1` 重新生成字体。脚本会加入配置的 Wi-Fi
名称所需字符；修改中文 Wi-Fi 名称后也应重新生成。
