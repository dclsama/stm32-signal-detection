# CAN 通信协议说明

## 物理层

- **收发器**: TJA1050 (高速 CAN)
- **速率**: 500 kbps
- **终端电阻**: 120Ω ×2 (CANH 和 CANL 之间, 总线两端)
- **连线**: 
  ```
  Board1 PA12(TX) → TJA1050 TXD → CANH/CANL
  Board1 PA11(RX) ← TJA1050 RXD ← CANH/CANL
       │                              │
       └────────── CAN Bus ───────────┘
       │                              │
  Board2 PA11(RX) ← TJA1050 RXD ← CANH/CANL
  Board2 PA12(TX) → TJA1050 TXD → CANH/CANL
  ```

## 帧格式定义

### 1. 传感器数据帧 (`CAN ID 0x201`)

| Byte | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | temp_int | int8 | 温度整数 (°C) |
| 1 | temp_dec | uint8 | 温度小数 (×10) |
| 2 | humi_int | uint8 | 湿度整数 (%RH) |
| 3 | humi_dec | uint8 | 湿度小数 (×10) |
| 4-5 | adc_value | uint16 (Big Endian) | ADC 原始值 (0-4095) |
| 6 | sensor_status | uint8 | bit0: DHT11_OK, bit1: ADC_OK |
| 7 | checksum | uint8 | Byte0-6 的 XOR 异或 |

### 2. 状态帧 (`CAN ID 0x202`)

| Byte | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | system_status | uint8 | 0x00=SAFE, 0x01=ERROR1, 0x02=ERROR2 |
| 1 | error_code | uint8 | 错误码 |

### 3. 确认帧 (`CAN ID 0x301`)

| Byte | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | ack | uint8 | 0xAA=接收正常, 0x55=请求重发 |

## 错误码定义

| 错误码 | 含义 |
|--------|------|
| 0x00 | 无错误 |
| 0x01 | DHT11 读取超时 |
| 0x02 | DHT11 校验错误 |
| 0x03 | ADC 故障 |
| 0x10 | CAN 接收超时 (>2s) |
| 0x11 | CAN Bus-Off |

## 超时与看门狗

| 监控项 | 超时 | 动作 |
|--------|------|------|
| DHT11 传感器 | 5s | 触发 ERROR1 |
| CAN 通信 | 2s | 触发 ERROR2 |
| IWDG 看门狗 | 4s | 硬件复位 |
