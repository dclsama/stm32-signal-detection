/**
 * @file    threshold.h
 * @brief   阈值管理 — 温度/2路电压告警判断
 * @note    ERROR1: 温度 > 阈值 (30°C)
 *          ERROR2: 任何一路电压 < 阈值 (1.50V)
 */

#ifndef __THRESHOLD_H__
#define __THRESHOLD_H__

#include "main.h"

#define TEMP_THRESHOLD_DEFAULT      30    /* °C */
#define VOLT_THRESHOLD_DEFAULT     150    /* x100, 1.50V */

typedef struct {
    int16_t  temp_high;
    uint16_t volt_low;
    uint8_t  enabled;
} ThresholdConfig_t;

typedef enum {
    THR_OK       = 0,
    THR_ERR_TEMP = 1,         /* → ERROR1 */
    THR_ERR_VOLT = 2,         /* → ERROR2 */
} ThresholdResult_t;

void Threshold_Init(void);
ThresholdResult_t Threshold_Check(int8_t temp_c, float volt1_v, float volt2_v);
ThresholdConfig_t* Threshold_GetConfig(void);
void Threshold_SetTemp(int16_t temp_c);
void Threshold_SetVolt(uint16_t volt_x100);
void Threshold_ClearAlarm(void);

#endif /* __THRESHOLD_H__ */
