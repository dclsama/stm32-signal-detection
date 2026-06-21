/**
 * @file    threshold.c
 * @brief   阈值管理 — 温度 + 2路电压告警
 */

#include "threshold.h"
#include "system_status.h"
#include "w25q64.h"
#include "cmsis_os.h"
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

void Threshold_IncTemp(void)
{
    config.temp_high++;
    printf("[Threshold] Temp limit: %d C\r\n", config.temp_high);
}

void Threshold_ResetTemp(void)
{
    config.temp_high = TEMP_THRESHOLD_DEFAULT;
    printf("[Threshold] Temp reset to default: %d C\r\n", config.temp_high);
}

void Threshold_ClearAlarm(void)
{
    SystemStatus_Set(SYSTEM_SAFE, ERR_NONE);
    printf("[Alarm] Cleared → SAFE\r\n");
}

/* ==================== Flash 持久化 ==================== */

extern osMutexId_t FlashMutex;   /* main.c 中定义 */

uint8_t Threshold_LoadFromFlash(void)
{
    ThresholdFlashData_t flash_data;

    /* 读取 Sector 0 数据 */
    W25Q64_ReadData(THRESHOLD_FLASH_ADDR, (uint8_t *)&flash_data,
                    sizeof(ThresholdFlashData_t));

    /* 验证 magic */
    if (flash_data.magic != THRESHOLD_MAGIC) {
        printf("[Threshold] Flash: no valid data "
               "(magic 0x%08lX != 0x%08lX), using defaults\r\n",
               (unsigned long)flash_data.magic,
               (unsigned long)THRESHOLD_MAGIC);
        return 0;
    }

    /* 验证 checksum: 字节 0~10 XOR 应等于字节 12 */
    uint8_t calc_cs = 0;
    uint8_t *p = (uint8_t *)&flash_data;
    for (uint8_t i = 0; i < 11; i++) {
        calc_cs ^= p[i];
    }
    if (calc_cs != flash_data.checksum) {
        printf("[Threshold] Flash: checksum fail "
               "(calc=0x%02X stored=0x%02X), using defaults\r\n",
               calc_cs, flash_data.checksum);
        return 0;
    }

    /* 加载到 RAM */
    config.temp_high = flash_data.temp_high;
    config.volt_low  = flash_data.volt_low;
    config.enabled   = flash_data.enabled;

    printf("[Threshold] Loaded from Flash: temp_high=%d C, "
           "volt_low=%d.%02dV, enabled=%d\r\n",
           config.temp_high, config.volt_low / 100, config.volt_low % 100,
           config.enabled);
    return 1;
}

void Threshold_SaveToFlash(void)
{
    ThresholdFlashData_t flash_data;
    uint8_t *p = (uint8_t *)&flash_data;

    /* 获取 Flash 互斥锁 */
    if (osMutexAcquire(FlashMutex, osWaitForever) != osOK) {
        printf("[Threshold] ERROR: Cannot acquire Flash mutex!\r\n");
        return;
    }

    /* 打包数据 */
    flash_data.magic     = THRESHOLD_MAGIC;
    flash_data.temp_high = config.temp_high;
    flash_data.volt_low  = config.volt_low;
    flash_data.enabled   = config.enabled;
    flash_data.reserved[0] = 0;
    flash_data.reserved[1] = 0;
    flash_data.reserved[2] = 0;

    /* 计算 checksum: 字节 0~10 的 XOR */
    flash_data.checksum = 0;
    for (uint8_t i = 0; i < 11; i++) {
        flash_data.checksum ^= p[i];
    }

    /* 擦除 Sector 0 并写入 */
    W25Q64_SectorErase(THRESHOLD_FLASH_ADDR);
    W25Q64_PageWrite(THRESHOLD_FLASH_ADDR, (uint8_t *)&flash_data,
                     sizeof(ThresholdFlashData_t));

    printf("[Threshold] Saved to Flash: temp_high=%d C, "
           "volt_low=%d.%02dV, enabled=%d, cs=0x%02X\r\n",
           config.temp_high, config.volt_low / 100, config.volt_low % 100,
           config.enabled, flash_data.checksum);

    osMutexRelease(FlashMutex);
}
