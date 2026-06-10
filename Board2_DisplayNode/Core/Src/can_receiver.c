/**
 * @file    can_receiver.c
 * @brief   Board2 CAN 接收器 — ISR 仅搬运数据, 处理在任务中
 */

#include "can_receiver.h"
#include <stdio.h>
#include <string.h>

CAN_ReceivedData_t g_can_rx_data = {0};

/* 中断缓冲 (双缓冲, ISR 只写, 任务只读) */
static CAN_SensorFrame_t irq_sensor_buf;
static CAN_StatusFrame_t irq_status_buf;
static uint8_t irq_sensor_pending = 0;
static uint8_t irq_status_pending = 0;

void CAN_Receiver_Init(void)
{
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDLIST;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = (CAN_ID_SENSOR_DATA << 5);
    filter.FilterIdLow  = (CAN_ID_STATUS << 5);
    filter.FilterMaskIdHigh = 0;
    filter.FilterMaskIdLow  = 0;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan, &filter);

    memset(&g_can_rx_data, 0, sizeof(g_can_rx_data));
    irq_sensor_pending = 0;
    irq_status_pending = 0;
}

void CAN_Receiver_Start(void)
{
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_Start(&hcan);
}

/* ==================== ISR 调用 — 仅搬运数据 ==================== */

void CAN_Receiver_ProcessRx(CAN_RxHeaderTypeDef *rx_header, uint8_t *rx_data)
{
    if (rx_header == NULL || rx_data == NULL) return;

    switch (rx_header->StdId) {
        case CAN_ID_SENSOR_DATA:
            memcpy(&irq_sensor_buf, rx_data, sizeof(CAN_SensorFrame_t));
            irq_sensor_pending = 1;
            break;

        case CAN_ID_STATUS:
            irq_status_buf.system_status = rx_data[0];
            irq_status_buf.error_code    = rx_data[1];
            irq_status_pending = 1;
            break;

        default:
            break;  /* 不认识的 ID 忽略 */
    }
}

/* ==================== 任务调用 — 处理 + 校验 + 打印 ==================== */

void CAN_Receiver_ProcessTask(void)
{
    if (irq_sensor_pending) {
        irq_sensor_pending = 0;

        /* XOR 校验 */
        if (CAN_Receiver_VerifyChecksum(&irq_sensor_buf) != 0) {
            printf("[CAN RX] Checksum Error!\r\n");
            return;
        }

        memcpy(&g_can_rx_data.sensor, &irq_sensor_buf,
               sizeof(CAN_SensorFrame_t));
        g_can_rx_data.sensor_updated = 1;

        printf("[CAN RX] T:%dC H:%d%% ADC:%d Stat:0x%02X\r\n",
               irq_sensor_buf.temp_int,
               irq_sensor_buf.humi_int,
               irq_sensor_buf.adc_value,
               irq_sensor_buf.sensor_status);
    }

    if (irq_status_pending) {
        irq_status_pending = 0;

        g_can_rx_data.status = irq_status_buf;
        g_can_rx_data.status_updated = 1;

        printf("[CAN RX] Status: %d Error: 0x%02X\r\n",
               irq_status_buf.system_status,
               irq_status_buf.error_code);
    }
}

uint8_t CAN_Receiver_VerifyChecksum(CAN_SensorFrame_t *frame)
{
    uint8_t calc = CAN_CalcChecksum((uint8_t *)frame, 7);
    return (calc == frame->checksum) ? 0 : 1;
}
