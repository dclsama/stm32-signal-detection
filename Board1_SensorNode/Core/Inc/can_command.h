/**
 * @file    can_command.h
 * @brief   Board1 (从机) CAN 命令接收 — 接收 0x301, 清告警并回复
 */

#ifndef __CAN_COMMAND_H__
#define __CAN_COMMAND_H__

#include "main.h"
#include "can_protocol.h"

/* ==================== API ==================== */

/**
 * @brief  初始化 CAN RX (过滤器仅收 0x301)
 */
void CAN_Command_Init(void);

/**
 * @brief  启动 CAN RX 中断
 */
void CAN_Command_Start(void);

/**
 * @brief  CAN RX ISR 处理 (仅拷贝数据到缓冲)
 */
void CAN_Command_ProcessRx(CAN_RxHeaderTypeDef *rx_header, uint8_t *rx_data);

/**
 * @brief  任务中处理收到的命令 (清告警 + 回复)
 */
void CAN_Command_ProcessTask(void);

#endif /* __CAN_COMMAND_H__ */
