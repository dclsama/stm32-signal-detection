/**
 * @file    system_status.c
 * @brief   系统状态管理实现 — 三级状态机 + 超时检测
 */

#include "system_status.h"
#include "FreeRTOS.h"
#include "task.h"

/* ==================== 内部变量 ==================== */
static SystemStatus_t current_status  = SYSTEM_SAFE;
static uint8_t        current_error   = ERR_NONE;
static TickType_t     last_can_tick   = 0;
static TickType_t     last_sensor_tick = 0;

/* ==================== API 实现 ==================== */

void SystemStatus_Init(void)
{
    current_status = SYSTEM_SAFE;
    current_error  = ERR_NONE;
    last_can_tick  = xTaskGetTickCount();
    last_sensor_tick = xTaskGetTickCount();
}

void SystemStatus_Set(SystemStatus_t status, uint8_t error_code)
{
    /* 告警锁存: ERROR2 只能被 Threshold_ClearAlarm (强制SAFE) 或自身恢复清除 */
    if (current_status == SYSTEM_ERROR2 && status == SYSTEM_ERROR1) {
        return;  /* ERROR2 不降级到 ERROR1 */
    }

    current_status = status;
    current_error  = error_code;
}

SystemStatus_t SystemStatus_Get(void)
{
    return current_status;
}

uint8_t SystemStatus_GetErrorCode(void)
{
    return current_error;
}

void SystemStatus_FeedCAN(void)
{
    last_can_tick = xTaskGetTickCount();
}

void SystemStatus_FeedSensor(void)
{
    last_sensor_tick = xTaskGetTickCount();
}

void SystemStatus_Check(void)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t can_elapsed, sensor_elapsed;

    /* 计算 elapsed, 处理 tick 溢出 */
    if (now >= last_can_tick) {
        can_elapsed = now - last_can_tick;
    } else {
        can_elapsed = (TickType_t)(-1) - last_can_tick + now + 1;
    }

    if (now >= last_sensor_tick) {
        sensor_elapsed = now - last_sensor_tick;
    } else {
        sensor_elapsed = (TickType_t)(-1) - last_sensor_tick + now + 1;
    }

    /* 检查 CAN 超时 */
    if (can_elapsed > pdMS_TO_TICKS(CAN_RX_TIMEOUT_MS)) {
        SystemStatus_Set(SYSTEM_ERROR2, ERR_CAN_TIMEOUT);
        return;  /* ERROR2 优先级最高 */
    }

    /* 检查传感器超时 */
    if (sensor_elapsed > pdMS_TO_TICKS(SENSOR_TIMEOUT_MS)) {
        SystemStatus_Set(SYSTEM_ERROR1, ERR_DHT11_TIMEOUT);
        return;
    }

    /* 所有看门狗正常 → 恢复 SAFE (从任何 ERROR 恢复) */
    if (current_status != SYSTEM_SAFE && (current_error == ERR_CAN_TIMEOUT || current_error == ERR_DHT11_TIMEOUT)) {
        SystemStatus_Set(SYSTEM_SAFE, ERR_NONE);
    }
}

const char* SystemStatus_GetString(void)
{
    switch (current_status) {
        case SYSTEM_SAFE:
            return "SAFE";
        case SYSTEM_ERROR1:
            if (current_error == ERR_TEMP_HIGH) return "ERROR1: Temp";
            return "ERROR1: Sensor";
        case SYSTEM_ERROR2:
            if (current_error == ERR_VOLT_LOW)  return "ERROR2: Volt";
            return "ERROR3: CAN";
        default:
            return "UNKNOWN";
    }
}
