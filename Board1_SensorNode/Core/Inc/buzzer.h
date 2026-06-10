/**
 * @file    buzzer.h
 * @brief   蜂鸣器驱动 — 有源蜂鸣器 (GPIO 开关控制)
 * @note    三种状态:
 *          BEEP_ON       — 持续鸣响
 *          BEEP_OFF      — 静默
 *          BEEP_INTERVAL — 间断鸣响 (200ms 响 / 500ms 停)
 */

#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "main.h"

/* ==================== 配置宏 ==================== */
#define BUZZER_GPIO_PORT      GPIOB
#define BUZZER_GPIO_PIN       GPIO_PIN_8

#define BUZZER_ON_MS          200     /* 间断模式: 鸣响时长 */
#define BUZZER_OFF_MS         500     /* 间断模式: 静默时长 */

/* ==================== 蜂鸣器状态枚举 ==================== */
typedef enum {
    BEEP_ON       = 0,   /* 持续鸣响 */
    BEEP_OFF      = 1,   /* 静默 */
    BEEP_INTERVAL = 2,   /* 间断鸣响 */
    BEEP_COUNT
} BuzzerState_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化蜂鸣器 GPIO
 */
void Buzzer_Init(void);

/**
 * @brief  设置蜂鸣器状态
 */
void Buzzer_SetState(BuzzerState_t state);

/**
 * @brief  切换到下一个状态 (循环)
 */
void Buzzer_CycleNext(void);

/**
 * @brief  获取当前状态
 */
BuzzerState_t Buzzer_GetState(void);

/**
 * @brief  蜂鸣器状态更新 (每 20ms 调用一次, 用于间断模式计时)
 */
void Buzzer_Tick(void);

#endif /* __BUZZER_H__ */
