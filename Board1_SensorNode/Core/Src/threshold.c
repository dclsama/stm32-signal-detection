/**
 * @file    threshold.c
 * @brief   阈值管理 — 温度 + 2路电压告警
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

ThresholdResult_t Threshold_Check(int8_t temp_c, float volt1_v, float volt2_v)
{
    if (!config.enabled) return THR_OK;

    /* ERROR1: 温度超限 */
    if (temp_c > config.temp_high) {
        return THR_ERR_TEMP;
    }

    /* ERROR2: 任何一路电压低于阈值 */
    uint16_t v1 = (uint16_t)(volt1_v * 100.0f);
    uint16_t v2 = (uint16_t)(volt2_v * 100.0f);
    if (v1 < config.volt_low || v2 < config.volt_low) {
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
    SystemStatus_Set(SYSTEM_SAFE, ERR_NONE);
    printf("[Alarm] Cleared → SAFE\r\n");
}
