/**
 * @file    key.c
 * @brief   按键驱动实现 — 状态机消抖
 *
 *          消抖原理:
 *          每隔 10ms 读一次按键电平，连续 3 次一致才判定状态变化。
 *          短按: 按下后 < 1s 释放
 *          长按: 按下后 ≥ 1s 触发一次
 */

#include "key.h"

/* ==================== 按键状态机 ==================== */
typedef enum {
    KS_IDLE,           /* 空闲, 等按下 */
    KS_PRESS_DETECT,   /* 检测到下降沿, 消抖确认 */
    KS_PRESSED,        /* 已确认按下, 计时 */
    KS_RELEASE_DETECT, /* 检测到释放, 消抖确认 */
} KeyState_t;

typedef struct {
    KeyState_t state;
    uint8_t    debounce_cnt;
    uint32_t   press_ms;         /* 按下持续时间 ms */
    uint8_t    short_pending;    /* 短按待确认 */
    uint8_t    long_triggered;   /* 长按已触发 */
    uint8_t    cur_level;
    uint8_t    last_level;
} KeyCtrl_t;

static KeyCtrl_t key1, key2;

/* ==================== 内部函数 ==================== */

static uint8_t Key_ReadPin(uint8_t key_id)
{
    if (key_id == 1) {
        return HAL_GPIO_ReadPin(KEY1_GPIO_PORT, KEY1_GPIO_PIN);
    } else {
        return HAL_GPIO_ReadPin(KEY2_GPIO_PORT, KEY2_GPIO_PIN);
    }
}

static KeyEvent_t Key_Process(KeyCtrl_t *k, uint8_t key_id)
{
    k->cur_level = Key_ReadPin(key_id);
    KeyEvent_t event = KEY_EVENT_NONE;

    switch (k->state) {

        case KS_IDLE:
            if (k->cur_level == GPIO_PIN_RESET && k->last_level == GPIO_PIN_SET) {
                /* 检测到下降沿 */
                k->state = KS_PRESS_DETECT;
                k->debounce_cnt = 0;
            }
            break;

        case KS_PRESS_DETECT:
            if (k->cur_level == GPIO_PIN_RESET) {
                k->debounce_cnt++;
                if (k->debounce_cnt >= KEY_DEBOUNCE_CNT) {
                    /* 消抖通过 → 进入按下 */
                    k->state = KS_PRESSED;
                    k->press_ms = 0;
                    k->long_triggered = 0;
                }
            } else {
                k->state = KS_IDLE;  /* 误触发 */
            }
            break;

        case KS_PRESSED:
            if (k->cur_level == GPIO_PIN_RESET) {
                k->press_ms += KEY_SCAN_MS;
                /* 长按检测 */
                if (!k->long_triggered && k->press_ms >= KEY_LONG_PRESS_MS) {
                    k->long_triggered = 1;
                    if (key_id == 2) event = KEY_EVENT_LONG_2;
                }
            } else {
                /* 上升沿 → 消抖确认 */
                k->state = KS_RELEASE_DETECT;
                k->debounce_cnt = 0;
            }
            break;

        case KS_RELEASE_DETECT:
            if (k->cur_level == GPIO_PIN_SET) {
                k->debounce_cnt++;
                if (k->debounce_cnt >= KEY_DEBOUNCE_CNT) {
                    /* 释放确认 */
                    if (key_id == 1 && k->press_ms < KEY_LONG_PRESS_MS) {
                        event = KEY_EVENT_SHORT_1;  /* 短按 */
                    }
                    if (key_id == 2) {
                        event = KEY_EVENT_RELEASE_2;
                    }
                    k->state = KS_IDLE;
                }
            } else {
                /* 又按下 → 回到持续按住 */
                k->state = KS_PRESSED;
            }
            break;
    }

    k->last_level = k->cur_level;
    return event;
}

/* ==================== API ==================== */

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = KEY1_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;   /* 上拉 → 默认高, 按下低 */
    HAL_GPIO_Init(KEY1_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = KEY2_GPIO_PIN;
    HAL_GPIO_Init(KEY2_GPIO_PORT, &GPIO_InitStruct);

    key1.state = key2.state = KS_IDLE;
    key1.last_level = key2.last_level = GPIO_PIN_SET;
}

KeyEvent_t Key_Scan(void)
{
    KeyEvent_t e1, e2;

    e1 = Key_Process(&key1, 1);
    if (e1 != KEY_EVENT_NONE) return e1;

    e2 = Key_Process(&key2, 2);

    /* 优先返回实际事件 (LONG_2 / RELEASE_2) */
    if (e2 != KEY_EVENT_NONE) return e2;

    /* 持续按下报告 */
    if (key2.state == KS_PRESSED) {
        return KEY_EVENT_PRESS_2;
    }

    return KEY_EVENT_NONE;
}