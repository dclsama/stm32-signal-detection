# CubeMX 工程配置指南

> 本指南教你如何使用 STM32CubeMX 创建 Board1 和 Board2 的 .ioc 工程。

---

## 通用配置 (两个板共用)

### 1. 芯片选择
- STM32F103C8T6 (LQFP48)

### 2. Pinout & Configuration

#### System Core
| 外设 | 配置 | 说明 |
|------|------|------|
| **RCC** | HSE = Crystal/Ceramic Resonator | 外部 8MHz 晶振 |
| **SYS** | Debug = Serial Wire | ST-Link 调试口 |
| | Timebase Source = TIM1 | FreeRTOS 用 SysTick, HAL 用 TIM1 |

#### Clock Configuration
```
HSE (8MHz) → PLL ×9 → SYSCLK = 72MHz
                         ├→ AHB  = 72MHz
                         ├→ APB1 = 36MHz (CAN 时钟源)
                         └→ APB2 = 72MHz
```

#### Middleware: FREERTOS
| 配置项 | 值 | 说明 |
|--------|-----|------|
| Interface | CMSIS_V2 | CMSIS-RTOS v2 API |
| Total Heap Size | 3072 (3KB, 默认) | 静态分配任务，堆仅用于队列/信号量，3KB 足够 |
| USE_TIMERS | Enabled | 软件定时器 |
| MAX_PRIORITIES | 7 | 优先级 0~6 |

#### Connectivity: USART1
| Pin | Mode |
|-----|------|
| PA9 | TX (Alternate Function Push-Pull) |
| PA10 | RX (Input) |
| Baud | 115200 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |

#### Timers: TIM3
| Channel | Pin | Mode |
|---------|-----|------|
| CH1 | PA6 | PWM Generation CH1 |
| CH2 | PA7 | PWM Generation CH2 |
| CH3 | PB0 | PWM Generation CH3 |
| Prescaler | 71 | 72MHz / 72 = 1MHz |
| Counter Period | 999 | 1MHz / 1000 = 1kHz PWM |

> ⚠️ STM32F103C8T6 的 PA5/PA6/PA7 没有 TIM2 通道，必须用 TIM3。
> PA6=TIM3_CH1, PA7=TIM3_CH2, PB0=TIM3_CH3

---

## Board1 专有配置

### Analog: ADC1
| 配置项 | 值 |
|--------|-----|
| IN1 (PA1) | 直接选 ADC1_IN1 即可 (F103 仅支持单端) |
| Continuous Conversion Mode | Enabled | 使能连续转换 |
| Scan Conversion Mode | Disabled | 单通道不扫描 |

### ADC1 DMA 配置
在 ADC1 → DMA Settings 标签页 → Add → 选 ADC1：

| 参数 | 值 |
|------|-----|
| DMA Request | ADC1 |
| Channel | DMA1 Channel 1 |
| Direction | Peripheral to Memory |
| Mode | **Circular** |
| Peripheral Data Width | Half Word |
| Memory Data Width | Half Word |
| Memory Address Increment | Enabled |


### Connectivity: SPI2
| Pin | Signal |
|-----|--------|
| PB13 | SCK |
| PB14 | MISO |
| PB15 | MOSI |
| Mode | Full-Duplex Master |
| Data Size | 8 Bits |
| CPOL/CPHA | Low / 1 Edge (Mode 0) |
| Prescaler | /4 → 9MHz (36MHz/4, W25Q64 最大 80MHz) |
| NSS | Software (PA4 手动控制) |

### Connectivity: CAN1
| Pin | Signal |
|-----|--------|
| PA11 | RX |
| PA12 | TX |
| Prescaler | 9 |
| BS1 | 5 tq |
| BS2 | 2 tq |
| SJW | 1 tq |
| → Baud Rate = 36MHz / 9 / (5+2+1) = 500 kbps |

---

## Board2 专有配置

### Connectivity: I2C1
| Pin | Signal |
|-----|--------|
| PB6 | SCL |
| PB7 | SDA |
| Speed | Fast Mode (400kHz) |
| Addressing | 7-bit |

### Connectivity: CAN1
- 同 Board1

---

## 导入用户代码

1. 将 `Core/Inc/` 中所有 `.h` 文件复制到 CubeMX 生成的 `Core/Inc/`
2. 将 `Core/Src/` 中所有 `.c` 文件复制到 CubeMX 生成的 `Core/Src/`
3. 替换 `Core/Src/main.c` (因为 CubeMX 会生成空的 main.c)
4. 在 CubeMX 的 Project Manager → Code Generator → 勾选:
   - ✅ "Generate peripheral initialization as a pair of .c/.h files"
   - ✅ "Keep User Code when re-generating"
5. 重新 Generate Code → 编译 → 烧录
