/**
 * @file    can_receiver.h
 * @brief   Board2 CAN 接收器 — 接收 Board1 数据帧和状态帧
 */

#ifndef __CAN_RECEIVER_H__
#define __CAN_RECEIVER_H__

#include "main.h"
#include "can_protocol.h"

/* ==================== 最新接收数据 (全局共享) ==================== */

/** Board2 接收到的传感器数据 (由 CAN_RX 任务更新) */
typedef struct {
    CAN_SensorFrame_t sensor;    /* 最新传感器数据帧 */
    CAN_StatusFrame_t status;    /* 最新状态帧 */
    uint8_t  sensor_updated;     /* 数据帧已更新标志 */
    uint8_t  status_updated;     /* 状态帧已更新标志 */
    uint32_t rx_count;           /* CAN 帧接收计数 (用于 OLED 诊断) */
} CAN_ReceivedData_t;

extern CAN_ReceivedData_t g_can_rx_data;

/* ==================== API 函数 ==================== */

/**
 * @brief  CAN 接收初始化 (配置过滤器)
 * @note   过滤器: ID List 模式, 仅接收 0x201 和 0x202
 */
void CAN_Receiver_Init(void);

/**
 * @brief  启动 CAN (进入 Normal 模式, 使能 FIFO0 中断)
 */
void CAN_Receiver_Start(void);

/**
 * @brief  CAN FIFO0 接收中断回调
 * @note   在 HAL_CAN_RxFifo0MsgPendingCallback 中调用
 */
void CAN_Receiver_ProcessRx(CAN_RxHeaderTypeDef *rx_header, uint8_t *rx_data);

/**
 * @brief  处理接收到的数据 (校验+打印), 在 Task_CAN_RX 中调用
 */
void CAN_Receiver_ProcessTask(void);

/**
 * @brief  验证数据帧校验和
 * @return 0=校验通过, 1=校验失败
 */
uint8_t CAN_Receiver_VerifyChecksum(CAN_SensorFrame_t *frame);

#endif /* __CAN_RECEIVER_H__ */
