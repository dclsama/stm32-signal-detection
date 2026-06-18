/**
 * @file    can_protocol.h
 * @brief   CAN 自定义通信协议定义
 * @note    供 Board1 (TX) 和 Board2 (RX) 共用
 *
 *          帧类型:
 *          ┌──────────┬─────────┬──────────────────┐
 *          │ CAN ID   │ 类型    │ 发送方           │
 *          ├──────────┼─────────┼──────────────────┤
 *          │ 0x201    │ 数据帧  │ Board1 → Board2  │
 *          │ 0x202    │ 状态帧  │ Board1 → Board2  │
 *          │ 0x301    │ 确认帧  │ Board2 → Board1  │
 *          └──────────┴─────────┴──────────────────┘
 */

#ifndef __CAN_PROTOCOL_H__
#define __CAN_PROTOCOL_H__

#include "main.h"
#include "dht11.h"
#include "adc_dma.h"
#include "system_status.h"

/* ==================== CAN ID 定义 ==================== */
#define CAN_ID_SENSOR_DATA    0x201    /* 传感器数据帧 */
#define CAN_ID_STATUS         0x202    /* 系统状态帧 */
#define CAN_ID_ACK            0x301    /* 确认/命令帧 */

/* ==================== CAN 配置 ==================== */
#define CAN_BAUD_RATE         500      /* 500 kbps */
/* STM32F103 CAN 波特率预分频计算 (APB1=36MHz):
 *   BaudRate = 36MHz / (Prescaler) / (BS1+BS2+SJW)
 *   500kbps:  36MHz / 9 / (5+2+1) = 500kHz  →  BRP=8, BS1=4, BS2=1
 *   实际值由 CubeMX 配置，此处仅作参考
 */

/* ==================== 数据帧格式 (CAN ID 0x201, 8 Byte) ==================== */
typedef struct __attribute__((packed)) {
    int8_t   temp_int;          /* Byte 0: 温度整数 (°C) */
    uint8_t  humi_int;          /* Byte 1: 湿度整数 (%RH) */
    uint8_t  adc1;              /* Byte 2: ADC1 8-bit (0-255 = 0-3.3V) */
    uint8_t  adc2;              /* Byte 3: ADC2 8-bit */
    uint8_t  sensor_status;     /* Byte 4: bit0=DHT11_OK, bit1=ADC1_OK, bit2=ADC2_OK */
    uint8_t  reserved[2];       /* Byte 5-6: 保留 */
    uint8_t  checksum;          /* Byte 7: XOR 校验 */
} CAN_SensorFrame_t;

/* 传感器状态位 */
#define SENSOR_OK_DHT11         0x01
#define SENSOR_OK_ADC1          0x02
#define SENSOR_OK_ADC2          0x04

/* ==================== 状态帧格式 (CAN ID 0x202, 2 Byte) ==================== */
typedef struct __attribute__((packed)) {
    uint8_t system_status;       /* Byte 0: 0=SAFE, 1=ERROR1, 2=ERROR2 */
    uint8_t error_code;          /* Byte 1: 具体错误码 */
} CAN_StatusFrame_t;

/* ==================== 确认帧格式 (CAN ID 0x301, 1 Byte) ==================== */
typedef struct __attribute__((packed)) {
    uint8_t ack;                 /* 0xAA=接收正常, 0x55=请求重发 */
} CAN_AckFrame_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  打包传感器数据帧
 * @param  frame   输出帧
 * @param  dht11   DHT11 数据
 * @param  adc     ADC 数据
 */
void CAN_PackSensorFrame(CAN_SensorFrame_t *frame,
                         DHT11_Data_t *dht11, ADC_Data_t *adc);

/**
 * @brief  计算 XOR 校验
 */
uint8_t CAN_CalcChecksum(uint8_t *data, uint8_t len);

/**
 * @brief  打包状态帧
 * @param  frame   输出帧
 * @param  status  系统状态
 * @param  error   错误码
 */
void CAN_PackStatusFrame(CAN_StatusFrame_t *frame,
                         SystemStatus_t status, uint8_t error);

/**
 * @brief  打印 CAN 数据帧到串口 (调试用)
 */
void CAN_PrintSensorFrame(CAN_SensorFrame_t *frame);

#endif /* __CAN_PROTOCOL_H__ */
