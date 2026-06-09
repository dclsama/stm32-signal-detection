/**
 * @file    can_protocol.c
 * @brief   CAN 自定义协议实现 — 数据打包/校验/调试打印
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
        frame->temp_dec  = 0;              /* DHT11 小数固定为 0 */
        frame->humi_int  = dht11->humidity;
        frame->humi_dec  = 0;
        frame->sensor_status |= SENSOR_OK_DHT11;
    } else {
        /* DHT11 故障: 填充 0xFF 表示无效 */
        frame->temp_int  = (int8_t)0xFF;
        frame->temp_dec  = 0xFF;
        frame->humi_int  = 0xFF;
        frame->humi_dec  = 0xFF;
    }

    if (adc && adc->status == 0) {
        frame->adc_value = adc->raw_value;
        frame->sensor_status |= SENSOR_OK_ADC;
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
                         SystemStatus_t status, uint8_t error)
{
    if (frame == NULL) return;

    frame->system_status = (uint8_t)status;
    frame->error_code    = error;
}

/* ==================== 调试打印 ==================== */

void CAN_PrintSensorFrame(CAN_SensorFrame_t *frame)
{
    if (frame == NULL) return;

    printf("[CAN TX] ");
    printf("Temp: %d.%dC  ", frame->temp_int, frame->temp_dec);
    printf("Humi: %d.%d%%  ", frame->humi_int, frame->humi_dec);
    printf("ADC: %d  ", frame->adc_value);
    printf("Status: 0x%02X  ", frame->sensor_status);
    printf("XOR: 0x%02X\r\n", frame->checksum);
}
