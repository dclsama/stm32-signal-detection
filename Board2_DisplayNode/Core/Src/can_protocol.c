/**
 * @file    can_protocol.c
 * @brief   CAN 自定义协议实现 — 2路ADC数据打包/校验/调试打印
 */

#include "can_protocol.h"
#include <stdio.h>
#include <string.h>

/* ==================== 数据帧打包 ==================== */

void CAN_PackSensorFrame(CAN_SensorFrame_t *frame,
                         DHT11_Data_t *dht11, ADC_Data_t *adc)
{
    if (frame == NULL) return;

    memset(frame, 0, sizeof(CAN_SensorFrame_t));

    if (dht11 && dht11->status == 0) {
        frame->temp_int  = (int8_t)dht11->temperature;
        frame->humi_int  = dht11->humidity;
        frame->sensor_status |= SENSOR_OK_DHT11;
    } else {
        frame->temp_int  = (int8_t)0xFF;
        frame->humi_int  = 0xFF;
    }

    /* 2 路 ADC: 12-bit → 8-bit 压缩 */
    if (adc && adc->ch1.status == 0) {
        frame->adc1 = (uint8_t)(adc->ch1.raw_value >> 4);
        frame->sensor_status |= SENSOR_OK_ADC1;
    }
    if (adc && adc->ch2.status == 0) {
        frame->adc2 = (uint8_t)(adc->ch2.raw_value >> 4);
        frame->sensor_status |= SENSOR_OK_ADC2;
    }

    /* XOR 校验: Byte 0~6 的异或 */
    frame->checksum = CAN_CalcChecksum((uint8_t *)frame, 7);
}

uint8_t CAN_CalcChecksum(uint8_t *data, uint8_t len)
{
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/* ==================== 状态帧打包 ==================== */

void CAN_PackStatusFrame(CAN_StatusFrame_t *frame,
                         SystemStatus_t status, uint8_t error,
                         int16_t temp_thr)
{
    if (frame == NULL) return;

    frame->system_status  = (uint8_t)status;
    frame->error_code     = error;
    frame->temp_threshold = temp_thr;
}

/* ==================== 调试打印 ==================== */

void CAN_PrintSensorFrame(CAN_SensorFrame_t *frame)
{
    if (frame == NULL) return;

    printf("[CAN TX] ");
    printf("Temp: %dC  ", frame->temp_int);
    printf("Humi: %d%%  ", frame->humi_int);
    printf("ADC1: %d ADC2: %d  ", frame->adc1, frame->adc2);
    printf("Stat: 0x%02X  ", frame->sensor_status);
    printf("ThrV: %d.%02dV  ",
           frame->volt_threshold / 100, frame->volt_threshold % 100);
    printf("XOR: 0x%02X\r\n", frame->checksum);
}
