# STM32 信号检测系统设计

> **26赛季 FSAE 电气组 STM32 实习任务**
>
> 开发者: [丁昶乐]
> 时间: 2026年6月

---

## 📋 项目简介

基于 STM32F103C8T6 双板 CAN 总线网络的信号检测系统，使用 FreeRTOS 实时操作系统。

- **Board 1 (传感器采集节点)**: DHT11 温湿度 + ADC 电位器 + W25Q64 Flash 存储，通过 CAN 总线发送数据
- **Board 2 (显示监控节点)**: CAN 接收 + OLED 显示 + RGB LED 状态指示

## 🏗️ 系统架构

```
Board1                              Board2
┌──────────────┐    CAN Bus    ┌──────────────┐
│ DHT11 传感器 │───────┬───────│ OLED 显示    │
│ ADC 电位器   │  500kbps   │ RGB LED      │
│ W25Q64 Flash │               │ 系统状态监控  │
│ RGB LED      │               │              │
│  FreeRTOS    │               │  FreeRTOS    │
└──────────────┘               └──────────────┘
```

## 🔧 开发环境

| 工具 | 版本/说明 |
|------|----------|
| IDE | VSCode + STM32 拓展 |
| 代码生成 | STM32CubeMX |
| 编译器 | arm-none-eabi-gcc |
| 调试器 | ST-Link V2 + OpenOCD |
| RTOS | FreeRTOS (CMSIS_V2) |
| 版本控制 | Git + GitHub |

## 📁 目录结构

```
stm32-signal-detection/
├── README.md
├── .gitignore
├── Board1_SensorNode/
│   └── Core/
│       ├── Inc/           ← 头文件
│       │   ├── dht11.h
│       │   ├── rgb_led.h
│       │   ├── adc_dma.h
│       │   ├── w25q64.h
│       │   ├── can_protocol.h
│       │   └── system_status.h
│       └── Src/           ← 源文件
│           ├── main.c     ← FreeRTOS 任务集成
│           ├── dht11.c
│           ├── rgb_led.c
│           ├── adc_dma.c
│           ├── w25q64.c
│           ├── can_protocol.c
│           └── system_status.c
├── Board2_DisplayNode/
│   └── Core/
│       ├── Inc/           ← 头文件 (含共享模块)
│       │   ├── oled.h
│       │   ├── can_receiver.h
│       │   ├── rgb_led.h
│       │   ├── can_protocol.h
│       │   └── system_status.h
│       └── Src/           ← 源文件
│           ├── main.c     ← FreeRTOS 任务集成
│           ├── oled.c
│           ├── can_receiver.c
│           ├── rgb_led.c
│           ├── can_protocol.c
│           └── system_status.c
└── docs/
    ├── CubeMX配置指南.md
    ├── CAN协议说明.md
    └── 硬件接线图.md
```

## 🚀 快速开始

### 1. 安装开发环境
- 安装 VSCode + STM32 拓展 + Git
- 安装 arm-none-eabi-gcc + OpenOCD
- 安装 STM32CubeMX

### 2. 创建 CubeMX 工程
详见 [docs/CubeMX配置指南.md](docs/CubeMX配置指南.md)

### 3. 导入驱动代码
将本仓库 `Core/Inc/` 和 `Core/Src/` 中的文件复制到 CubeMX 生成的工程对应位置

### 4. 编译与烧录
```bash
# VSCode: F1 → STM32: Build
# 或命令行:
make -j4
# 烧录:
STM32: Flash (via ST-Link)
```

### 5. 验证
- 打开串口助手 (115200 8N1)
- Board1 应打印 DHT11 温湿度和 ADC 值
- Board2 OLED 应显示接收到的数据
- 系统状态: SAFE(绿灯) / ERROR1(黄灯) / ERROR2(红灯)

## 📦 硬件清单

| 物料 | 数量 | 说明 |
|------|------|------|
| STM32F103C8T6 | 2 | 最小系统板 (蓝色/黑色) |
| TJA1050 模块 | 2 | CAN 收发器 |
| DHT11 | 1 | 温湿度传感器 |
| SSD1306 OLED | 1 | 0.96" I2C 128×64 |
| W25Q64 模块 | 1 | SPI Flash 8MB |
| 共阴 RGB LED | 2 | 3 色 LED |
| 10k 电位器 | 1 | ADC 模拟输入 |
| 120Ω 电阻 | 2 | CAN 终端电阻 |
| 220Ω 电阻 | 3 | RGB LED 限流 (可选) |
| 面包板 + 杜邦线 | 若干 | |
| ST-Link V2 | 1 | 下载调试器 |
| CH340 串口模块 | 1 | USB 转 TTL |

## 📄 CAN 协议

详见 [docs/CAN协议说明.md](docs/CAN协议说明.md)

| CAN ID | 帧类型 | 方向 |
|--------|--------|------|
| 0x201 | 传感器数据帧 | Board1 → Board2 |
| 0x202 | 系统状态帧 | Board1 → Board2 |
| 0x301 | 确认/命令帧 | Board2 → Board1 |

## 📊 系统状态机

```
SAFE (绿灯) ──传感器故障──→ ERROR1 (黄灯)
   │                            │
   └── CAN超时 ──→ ERROR2 (红灯) ←──┘
         ↑_____________恢复___________↓
```

---

> SCUT Racing ES Team | SCUTRacingES@outlook.com
