/**
 * @file    can_command.c
 * @brief   Board1 (从机) CAN 命令接收实现
 *          ISR 仅拷贝数据, 任务进行清告警+回复处理
 */

#include "can_command.h"
#include "system_status.h"
#include "threshold.h"
#include <stdio.h>

/* ISR 缓冲 */
static uint8_t cmd_pending = 0;

void CAN_Command_Init(void)
{
    /* 过滤器: 仅接收 0x301 */
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank = 1;              /* Board1 用 Bank 1 (0 给 TX) */
    filter.FilterMode = CAN_FILTERMODE_IDLIST;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = (CAN_ID_ACK << 5);
    filter.FilterIdLow  = (CAN_ID_ACK << 5);
    filter.FilterMaskIdHigh = 0;
    filter.FilterMaskIdLow  = 0;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan, &filter);

    cmd_pending = 0;
}

void CAN_Command_Start(void)
{
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

/* ==================== ISR 调用 — 仅拷贝 ==================== */

void CAN_Command_ProcessRx(CAN_RxHeaderTypeDef *rx_header, uint8_t *rx_data)
{
    if (rx_header == NULL) return;

    if (rx_header->StdId == CAN_ID_ACK) {
        cmd_pending = 1;
    }
}

/* ==================== 任务调用 — 处理命令 ==================== */

void CAN_Command_ProcessTask(void)
{
    if (!cmd_pending) return;
    cmd_pending = 0;

    /* 收到 0x301 → 清除告警 */
    Threshold_ClearAlarm();
    printf("[CAN CMD] Received 0x301 → Alarm Cleared\r\n");

    /* 回复确认帧 (0x301, DLC=1, data=0xBB 表示完成) */
    CAN_TxHeaderTypeDef tx_header = {0};
    uint8_t reply = 0xBB;  /* ACK: 清除完成 */
    uint32_t tx_mailbox;

    tx_header.StdId = CAN_ID_ACK;
    tx_header.ExtId = 0;
    tx_header.IDE   = CAN_ID_STD;
    tx_header.RTR   = CAN_RTR_DATA;
    tx_header.DLC   = 1;

    if (HAL_CAN_AddTxMessage(&hcan, &tx_header, &reply, &tx_mailbox) == HAL_OK) {
        printf("[CAN CMD] Reply 0x301:0xBB sent\r\n");
    } else {
        printf("[CAN CMD] Reply Failed!\r\n");
    }
}
