/**
 * @file    threshold.c
 * @brief   阈值管理实现
 */

#include "threshold.h"
#include "system_status.h"
#include <stdio.h>

static ThresholdConfig_t config = {
    .temp_high = TEMP_THRESHOLD_DEFAULT,
    .volt_low  = VOLT_THRESHOLD_DEFAULT,
    .enabled   = 1
};

void Threshold_Init(void)
{
    config.temp_high = TEMP_THRESHOLD_DEFAULT;
    config.volt_low  = VOLT_THRESHOLD_DEFAULT;
    config.enabled   = 1;
}

ThresholdResult_t Threshold_Check(int8_t temp_c, float volt_v)
{
    if (!config.enabled) return THR_OK;

    uint16_t volt_x100 = (uint16_t)(volt_v * 100.0f);

    /* ERROR1: 温度超限 */
    if (temp_c > config.temp_high) {
        return THR_ERR_TEMP;
    }

    /* ERROR2: 电压过低 */
    if (volt_x100 < config.volt_low) {
        return THR_ERR_VOLT;
    }

    return THR_OK;
}

ThresholdConfig_t* Threshold_GetConfig(void)
{
    return &config;
}

void Threshold_SetTemp(int16_t temp_c)
{
    config.temp_high = temp_c;
    printf("[Threshold] Temp limit: %d C\r\n", temp_c);
}

void Threshold_SetVolt(uint16_t volt_x100)
{
    config.volt_low = volt_x100;
    printf("[Threshold] Volt limit: %d.%02dV\r\n",
           volt_x100 / 100, volt_x100 % 100);
}

void Threshold_ClearAlarm(void)
{
    /* 清除告警, 恢复 SAFE */
    SystemStatus_Set(SYSTEM_SAFE, ERR_NONE);
    printf("[Alarm] Cleared → SAFE\r\n");
}
