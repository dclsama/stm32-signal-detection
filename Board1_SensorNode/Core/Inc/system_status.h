/**
 * @file    system_status.h
 * @brief   系统状态管理 — SAFE / ERROR1 / ERROR2 三级状态机
 * @note    供 Board1 和 Board2 共用
 */

#ifndef __SYSTEM_STATUS_H__
#define __SYSTEM_STATUS_H__

#include "main.h"

/* ==================== 系统状态枚举 ==================== */
typedef enum {
    SYSTEM_SAFE   = 0x00,       /* 正常 */
    SYSTEM_ERROR1 = 0x01,       /* 传感器故障 (DHT11/ADC 超时或无效) */
    SYSTEM_ERROR2 = 0x02        /* CAN 通信故障 (接收超时) */
} SystemStatus_t;

/* ==================== 错误码 ==================== */
#define ERR_NONE                0x00
#define ERR_DHT11_TIMEOUT       0x01
#define ERR_DHT11_CHECKSUM      0x02
#define ERR_ADC_FAIL            0x03
#define ERR_TEMP_HIGH           0x04  /* 温度超阈值 */
#define ERR_VOLT_LOW            0x05  /* 电压低于阈值 */
#define ERR_CAN_TIMEOUT         0x10
#define ERR_CAN_BUSOFF          0x11
#define ERR_CAN_TEC_HIGH        0x12  /* TEC>127 硬件错误 */

/* ==================== 配置 ==================== */
#define CAN_RX_TIMEOUT_MS       5000  /* CAN 接收超时 (ms) → 容忍偶发丢帧 */
#define SENSOR_TIMEOUT_MS       5000  /* 传感器超时 (ms) → 触发 ERROR1 */

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化系统状态 (默认为 SAFE)
 */
void SystemStatus_Init(void);

/**
 * @brief  设置系统状态
 * @param  status  新状态
 * @param  error_code  错误码 (仅在 ERROR1/2 时有意义)
 */
void SystemStatus_Set(SystemStatus_t status, uint8_t error_code);

/**
 * @brief  获取当前系统状态
 * @return 当前状态枚举值
 */
SystemStatus_t SystemStatus_Get(void);

/**
 * @brief  获取当前错误码
 */
uint8_t SystemStatus_GetErrorCode(void);

/**
 * @brief  更新 CAN 接收时间戳 (由 CAN RX 任务调用)
 */
void SystemStatus_FeedCAN(void);

/**
 * @brief  更新传感器数据时间戳 (由传感器任务调用)
 */
void SystemStatus_FeedSensor(void);

/**
 * @brief  检查看门狗超时 (由监控任务周期性调用)
 * @note   如果 CAN 或传感器超时，自动切换为对应 ERROR 状态
 */
void SystemStatus_Check(void);

/**
 * @brief  获取状态描述字符串 (用于 OLED 显示)
 */
const char* SystemStatus_GetString(void);

#endif /* __SYSTEM_STATUS_H__ */
