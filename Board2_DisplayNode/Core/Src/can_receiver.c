/**
 * @file    can_receiver.c
 * @brief   Board2 CAN 接收器实现
 */

#include "can_receiver.h"
#include <stdio.h>
#include <string.h>

/* ==================== 全局接收数据 ==================== */
CAN_ReceivedData_t g_can_rx_data = {0};

/* ==================== 初始化 ==================== */

void CAN_Receiver_Init(void)
{
    /* 配置 CAN 过滤器: 只接收 ID 0x201 和 0x202 */
    CAN_FilterTypeDef filter = {0};

    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDLIST;     /* 标识符列表模式 */
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = (CAN_ID_SENSOR_DATA << 5);     /* ID1 = 0x201 */
    filter.FilterIdLow  = (CAN_ID_STATUS << 5);          /* ID2 = 0x202 */
    filter.FilterMaskIdHigh = 0;
    filter.FilterMaskIdLow  = 0;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;

    HAL_CAN_ConfigFilter(&hcan, &filter);

    memset(&g_can_rx_data, 0, sizeof(g_can_rx_data));
}

void CAN_Receiver_Start(void)
{
    /* 使能 FIFO0 消息挂起中断 */
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

    /* 启动 CAN */
    HAL_CAN_Start(&hcan);
}

/* ==================== RX 中断处理 ==================== */

void CAN_Receiver_ProcessRx(CAN_RxHeaderTypeDef *rx_header, uint8_t *rx_data)
{
    if (rx_header == NULL || rx_data == NULL) return;

    switch (rx_header->StdId) {
        case CAN_ID_SENSOR_DATA:
        {
            /* 解析传感器数据帧 */
            CAN_SensorFrame_t *frame =
                (CAN_SensorFrame_t *)rx_data;

            /* XOR 校验 */
            if (CAN_Receiver_VerifyChecksum(frame) != 0) {
                printf("[CAN RX] Checksum Error!\r\n");
                return;
            }

            /* 保存数据 */
            memcpy(&g_can_rx_data.sensor, frame,
                   sizeof(CAN_SensorFrame_t));
            g_can_rx_data.sensor_updated = 1;

            /* 调试打印 */
            printf("[CAN RX] T:%d.%dC H:%d.%d%% ADC:%d Stat:0x%02X\r\n",
                   frame->temp_int, frame->temp_dec,
                   frame->humi_int, frame->humi_dec,
                   frame->adc_value,
                   frame->sensor_status);
            break;
        }

        case CAN_ID_STATUS:
        {
            /* 解析状态帧 */
            CAN_StatusFrame_t *frame =
                (CAN_StatusFrame_t *)rx_data;

            memcpy(&g_can_rx_data.status, frame,
                   sizeof(CAN_StatusFrame_t));
            g_can_rx_data.status_updated = 1;

            printf("[CAN RX] Status: %d Error: 0x%02X\r\n",
                   frame->system_status, frame->error_code);
            break;
        }

        default:
            printf("[CAN RX] Unknown ID: 0x%03X\r\n",
                   rx_header->StdId);
            break;
    }
}

uint8_t CAN_Receiver_VerifyChecksum(CAN_SensorFrame_t *frame)
{
    uint8_t calc = CAN_CalcChecksum((uint8_t *)frame, 7);
    return (calc == frame->checksum) ? 0 : 1;
}
