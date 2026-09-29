# ESP32-S3 AMOLED 服务器数据显示终端 — 项目资料

> 本文档仅记录项目背景、技术基线与硬件设备参数，供 Coding Agent / Harness 读取。  
> 设备型号：**Waveshare ESP32-S3-Touch-AMOLED-1.43**（非 1.43C）  
> 参数核对日期：2026-09-29

## 1. 项目概述

项目目标是在 Waveshare ESP32-S3-Touch-AMOLED-1.43 开发板上运行自定义图形界面，通过 Wi-Fi 获取服务器数据并显示在 1.43 英寸 AMOLED 屏幕上。


## 2. 项目技术基线

| 项目 | 选型 |
|---|---|
| MCU 平台 | ESP32-S3 |
| 开发框架 | ESP-IDF 6.1 |
| 编程语言 | C / C++ |
| GUI | LVGL 9.x |
| 显示接口 | QSPI |
| 触摸接口 | I2C |
| 网络 | 2.4 GHz Wi-Fi |
| 服务器通信 | HTTP/HTTPS JSON，可扩展 MQTT |
| 数据格式 | JSON |
| 目标屏幕分辨率 | 466 × 466 |

项目代码应以 ESP-IDF 6.1 API 和组件体系为基础。

## 3. 开发板型号

设备：

```text
Waveshare ESP32-S3-Touch-AMOLED-1.43
```

微雪同时提供：

```text
ESP32-S3-Touch-AMOLED-1.43
ESP32-S3-Touch-AMOLED-1.43-B
```

其中 `-B` 为带外壳版本。

本项目设备不属于 `ESP32-S3-Touch-AMOLED-1.43C` 系列，注意不要混用 1.43C 的硬件参数。

## 4. ESP32-S3 核心参数

| 参数 | 规格 |
|---|---|
| SoC | ESP32-S3R8 |
| CPU 架构 | Xtensa 32-bit LX7 双核 |
| 最高主频 | 240 MHz |
| SRAM | 512 KB |
| ROM | 384 KB |
| Flash | 16 MB |
| PSRAM | 8 MB |
| Wi-Fi | 2.4 GHz IEEE 802.11 b/g/n |
| Bluetooth | Bluetooth 5 LE |
| 板载天线 | 陶瓷天线 |
| 外部天线 | IPEX1，需调整焊点后使用 |

## 5. AMOLED 屏幕参数

| 参数 | 规格 |
|---|---|
| 显示类型 | AMOLED |
| 屏幕尺寸 | 1.43 inch |
| 分辨率 | 466 × 466 |
| 显示颜色 | 16.7M |
| 亮度 | 350 cd/m² |
| 对比度 | 60000:1 |
| 通信接口 | QSPI |
| 显示驱动 IC | SH8601 / CO5300 |
| 触摸 | 电容触摸 |
| 触摸控制器 | FT3168 |
| 触摸通信接口 | I2C |

注意：该型号存在不同 LCD Driver IC，开发时需要确认实际硬件使用的是 **SH8601** 还是 **CO5300**，不能默认两者完全一致。

## 6. AMOLED / QSPI 引脚

| GPIO | 功能 |
|---:|---|
| GPIO9 | QSPI_CS |
| GPIO10 | QSPI_CLK |
| GPIO11 | QSPI_D0 |
| GPIO12 | QSPI_D1 |
| GPIO13 | QSPI_D2 |
| GPIO14 | QSPI_D3 |
| GPIO21 | AMOLED_RST |
| GPIO42 | AMOLED_EN |

建议项目内统一使用对应的板级宏：

```c
#define BOARD_LCD_H_RES       466
#define BOARD_LCD_V_RES       466

#define BOARD_LCD_CS          9
#define BOARD_LCD_CLK         10
#define BOARD_LCD_DATA0       11
#define BOARD_LCD_DATA1       12
#define BOARD_LCD_DATA2       13
#define BOARD_LCD_DATA3       14

#define BOARD_LCD_RST         21
#define BOARD_LCD_EN          42
```

## 7. I2C 总线

板载 I2C：

| GPIO | 功能 |
|---:|---|
| GPIO47 | SDA |
| GPIO48 | SCL |

该 I2C 总线由多个板载器件共享：

- FT3168 触摸控制器
- QMI8658 IMU
- PCF85063 RTC

对应宏：

```c
#define BOARD_I2C_SDA         47
#define BOARD_I2C_SCL         48
```

## 8. 触摸参数

| 参数 | 规格 |
|---|---|
| 类型 | 电容触摸 |
| 控制器 | FT3168 |
| 接口 | I2C |
| SDA | GPIO47 |
| SCL | GPIO48 |
| 屏幕坐标范围 | 466 × 466 |

触摸和 IMU、RTC 共用板载 I2C 总线。

## 9. IMU

板载姿态传感器：

```text
QMI8658
```

主要能力：

- 三轴加速度计
- 三轴陀螺仪
- 姿态和运动检测

接口：

```text
I2C
SDA = GPIO47
SCL = GPIO48
IMU_INT = GPIO8
```

当前服务器数据显示功能不依赖 IMU，但属于开发板已有硬件资源。

## 10. RTC

板载 RTC：

```text
PCF85063
```

接口：

```text
I2C
SDA = GPIO47
SCL = GPIO48
RTC_INT = GPIO15
```

开发板提供 RTC 独立电源接口。

## 11. Micro SD

开发板背面带 Micro SD 卡槽。

| GPIO | SD 功能 |
|---:|---|
| GPIO38 | SD_CS |
| GPIO39 | SD_MOSI |
| GPIO40 | SD_MISO |
| GPIO41 | SD_SCLK |

当前项目可以不启用 SD，但这些 GPIO 已被板载 SD 接口占用。

## 12. UART

| GPIO | 功能 |
|---:|---|
| GPIO43 | UART_TXD |
| GPIO44 | UART_RXD |

## 13. USB

| GPIO | 功能 |
|---:|---|
| GPIO19 | USB_N |
| GPIO20 | USB_P |

开发板带 USB Type-C 接口，可用于程序烧录和 USB 通信。

## 14. 其他 GPIO / 板载功能

| GPIO / 信号 | 功能 |
|---|---|
| GPIO0 | BOOT0 |
| GPIO4 | BAT_ADC |
| GPIO8 | IMU_INT |
| GPIO15 | RTC_INT |
| EN | RESET |

板载按键：

- BOOT
- Reset

## 15. 电源相关

开发板包含：

- USB Type-C 供电
- 3.7V 锂电池接口
- 电池充电功能
- 充电指示灯
- 电源指示灯
- RTC 独立电源接口

电池电压可通过：

```text
GPIO4 / BAT_ADC
```

进行采样。

## 16. 板载资源汇总

开发板主要资源：

```text
ESP32-S3R8
16 MB Flash
8 MB PSRAM
1.43" 466×466 AMOLED
SH8601 / CO5300 LCD Driver
FT3168 Touch
QMI8658 IMU
PCF85063 RTC
Micro SD
3.7V Battery Interface
USB Type-C
UART
I2C
BOOT Button
Reset Button
Ceramic Antenna
IPEX1 External Antenna Interface
```

## 17. 项目服务器数据模型

服务器端计划提供设备状态数据，例如：

```json
{
  "server": "edge-server-01",
  "status": "online",
  "cpu": 23.6,
  "memory": 61.2,
  "disk": 47.8,
  "temperature": 42.3,
  "uptime": 172800
}
```

对应 ESP32 侧可抽象为：

```c
typedef struct {
    char server[32];
    bool online;

    float cpu;
    float memory;
    float disk;
    float temperature;

    uint32_t uptime;
} server_status_t;
```

字段可根据实际服务器接口调整。

## 18. 预期界面数据

显示内容主要包括：

```text
服务器名称
在线 / 离线状态
CPU 使用率
内存使用率
磁盘使用率
温度
运行时间
最后更新时间
告警状态
```

界面目标分辨率固定为：

```text
466 × 466
```

## 19. 项目边界

当前项目定位是：

```text
ESP32-S3 + AMOLED + LVGL
             │
             ├── Wi-Fi
             ├── HTTP / HTTPS
             ├── JSON
             ├── MQTT（可选）
             └── Touch
```

项目不是通用开发板测试程序，不需要为了使用开发板而强制启用全部板载外设。

## 20. 官方资料

Waveshare 产品文档：

```text
https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-1.43/
```

Waveshare 相关资料：

```text
https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.43/Resources-And-Documents
```

ESP-IDF：

```text
https://docs.espressif.com/projects/esp-idf/
```

Espressif Component Registry：

```text
https://components.espressif.com/
```

LVGL：

```text
https://docs.lvgl.io/
```

## 21. Agent 需要掌握的关键事实

```text
设备型号：
Waveshare ESP32-S3-Touch-AMOLED-1.43

不是：
ESP32-S3-Touch-AMOLED-1.43C

MCU：
ESP32-S3R8

Flash：
16 MB

PSRAM：
8 MB

屏幕：
1.43" AMOLED

分辨率：
466 × 466

LCD 接口：
QSPI

LCD Driver：
SH8601 / CO5300

Touch：
FT3168

Touch 接口：
I2C

共享 I2C：
GPIO47 SDA
GPIO48 SCL

开发框架：
ESP-IDF 6.1

GUI：
LVGL 9.x

项目用途：
通过 Wi-Fi 获取服务器数据并在 AMOLED 上显示服务器状态 Dashboard
```
