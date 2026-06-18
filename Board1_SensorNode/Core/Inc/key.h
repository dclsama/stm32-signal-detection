/**
 * @file    key.h
 * @brief   按键驱动 — 软件消抖 + 短按/长按检测
 * @note    扫描周期 10ms, 消抖确认 3 次 (30ms)
 *          长按阈值 1000ms
 */

#ifndef __KEY_H__
#define __KEY_H__

#include "main.h"

/* ==================== 配置宏 ==================== */
#define KEY1_GPIO_PORT        GPIOB
#define KEY1_GPIO_PIN         GPIO_PIN_1

#define KEY2_GPIO_PORT        GPIOA
#define KEY2_GPIO_PIN         GPIO_PIN_2      /* PA2 (PB2=BOOT1 不可用作GPIO) */

/* 消抖参数 */
#define KEY_SCAN_MS           10      /* 扫描周期 */
#define KEY_DEBOUNCE_CNT      3       /* 连续 N 次 = 稳定 */
#define KEY_LONG_PRESS_MS     1000    /* 长按阈值 */

/* ==================== 按键事件类型 ==================== */
typedef enum {
    KEY_EVENT_NONE        = 0,
    KEY_EVENT_SHORT_1     = 1,       /* 按键1 短按 */
    KEY_EVENT_LONG_2      = 2,       /* 按键2 长按 */
    KEY_EVENT_PRESS_2     = 3,       /* 按键2 按下 (持续) */
    KEY_EVENT_RELEASE_2   = 4,       /* 按键2 释放 */
} KeyEvent_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化按键 GPIO (输入上拉)
 */
void Key_Init(void);

/**
 * @brief  按键扫描任务 (每 10ms 调用一次)
 * @return 触发的事件类型
 */
KeyEvent_t Key_Scan(void);

#endif /* __KEY_H__ */