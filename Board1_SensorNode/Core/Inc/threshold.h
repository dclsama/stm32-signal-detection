/**
 * @file    threshold.h
 * @brief   阈值管理 — 温度/电压告警判断
 * @note    ERROR1: 温度 > 温度阈值 (可设 30°C)
 *          ERROR2: 电压 < 电压阈值 (可设 1.5V)
 */

#ifndef __THRESHOLD_H__
#define __THRESHOLD_H__

#include "main.h"

/* ==================== 默认阈值 ==================== */
#define TEMP_THRESHOLD_DEFAULT      30    /* °C,  温度高于此值 → ERROR1 */
#define VOLT_THRESHOLD_DEFAULT     150    /* ×100, 1.50V, 电压低于此值 → ERROR2 */

/* ==================== 数据结构 ==================== */
typedef struct {
    int16_t  temp_high;       /* 温度阈值 ×1 (°C) */
    uint16_t volt_low;        /* 电压阈值 ×100 (V) */
    uint8_t  enabled;         /* 阈值检测使能 */
} ThresholdConfig_t;

/* ==================== 阈值检查结果 ==================== */
typedef enum {
    THR_OK      = 0,          /* 均正常 */
    THR_ERR_TEMP = 1,         /* 温度超限 → ERROR1 */
    THR_ERR_VOLT = 2,         /* 电压过低 → ERROR2 */
} ThresholdResult_t;

/* ==================== API 函数 ==================== */

void Threshold_Init(void);

/**
 * @brief  检查温度-电压是否超限
 * @param  temp_c  温度值 (°C)
 * @param  volt_v  电压值 (V float)
 * @return 检查结果
 */
ThresholdResult_t Threshold_Check(int8_t temp_c, float volt_v);

/**
 * @brief  获取阈值配置
 */
ThresholdConfig_t* Threshold_GetConfig(void);

/**
 * @brief  设置温度阈值
 */
void Threshold_SetTemp(int16_t temp_c);

/**
 * @brief  设置电压阈值
 */
void Threshold_SetVolt(uint16_t volt_x100);

/**
 * @brief  清除告警状态 (按键2长按)
 */
void Threshold_ClearAlarm(void);

#endif /* __THRESHOLD_H__ */
