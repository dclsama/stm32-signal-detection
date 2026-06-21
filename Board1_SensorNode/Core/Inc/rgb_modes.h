/**
 * @file    rgb_modes.h
 * @brief   RGB LED 多模式控制器
 * @note    模式列表:
 *          MODE_GREEN_SOLID    — 绿色常亮
 *          MODE_RED_SOLID      — 红色常亮
 *          MODE_GREEN_BREATHE  — 绿色呼吸灯
 *          MODE_OFF            — 全灭
 *
 *          呼吸灯使用正弦波计算亮度, 周期约 2 秒
 */

#ifndef __RGB_MODES_H__
#define __RGB_MODES_H__

#include "rgb_led.h"

/* ==================== RGB 模式枚举 ==================== */
typedef enum {
    MODE_GREEN_SOLID    = 0,
    MODE_RED_SOLID      = 1,
    MODE_GREEN_BREATHE  = 2,
    MODE_OFF            = 3,
    MODE_COUNT
} RGB_Mode_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  切换到指定模式
 */
void RGB_ModeSet(RGB_Mode_t mode);

/**
 * @brief  切换到下一个模式 (循环)
 */
void RGB_ModeCycleNext(void);

/**
 * @brief  获取当前模式
 */
RGB_Mode_t RGB_ModeGet(void);

/**
 * @brief  呼吸灯更新 (每 20ms 调用一次)
 */
void RGB_ModeBreatheTick(void);

void RGB_ModeKey2Control(uint8_t pressed);

/**
 * @brief  查询 KEY2 是否正在覆盖 RGB (按住状态)
 * @return 1=KEY2 按住中, 0=正常
 */
uint8_t RGB_IsKey2Override(void);

/**
 * @brief  红色呼吸灯 tick (告警用, 与绿色呼吸共用查表)
 */
void RGB_AlarmRedBreatheTick(void);

#endif /* __RGB_MODES_H__ */
