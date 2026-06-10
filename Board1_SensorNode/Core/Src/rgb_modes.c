/**
 * @file    rgb_modes.c
 * @brief   RGB LED 多模式控制器实现
 */

#include "rgb_modes.h"
#include <math.h>

/* ==================== 内部变量 ==================== */
static RGB_Mode_t current_mode = MODE_GREEN_SOLID;

/* 呼吸灯参数 */
#define BREATHE_PERIOD_MS    2000      /* 周期 2s */
static uint32_t breathe_tick = 0;

/* 按键2 覆盖标志：true 时模式切换被暂停 */
static uint8_t key2_override = 0;

/* ==================== API 实现 ==================== */

void RGB_ModeSet(RGB_Mode_t mode)
{
    current_mode = mode;

    switch (mode) {
        case MODE_GREEN_SOLID:
            RGB_SetColor(0, 999, 0);       /* 绿 */
            break;
        case MODE_RED_SOLID:
            RGB_SetColor(999, 0, 0);       /* 红 */
            break;
        case MODE_GREEN_BREATHE:
            breathe_tick = 0;
            break;
        case MODE_OFF:
        default:
            RGB_Off();
            break;
    }
}

void RGB_ModeCycleNext(void)
{
    if (key2_override) return;            /* 按键2 占用时不切换 */

    RGB_Mode_t next = (current_mode + 1) % MODE_COUNT;
    RGB_ModeSet(next);
}

RGB_Mode_t RGB_ModeGet(void)
{
    return current_mode;
}

void RGB_ModeBreatheTick(void)
{
    if (current_mode != MODE_GREEN_BREATHE) return;
    if (key2_override) return;

    breathe_tick += 20;  /* 每 20ms */

    /* 正弦波: 0→π→2π 映射到 0→999 */
    float phase = (float)(breathe_tick % BREATHE_PERIOD_MS)
                  / BREATHE_PERIOD_MS * 2.0f * 3.14159f;
    /* sin: -1~1 → 转为 0~1 */
    float brightness = (sinf(phase) + 1.0f) / 2.0f;

    uint16_t g = (uint16_t)(brightness * 999.0f);
    RGB_SetColor(0, g, 0);               /* 仅绿色呼吸 */
}

void RGB_ModeKey2Control(uint8_t pressed)
{
    if (pressed) {
        key2_override = 1;
        RGB_SetColor(0, 999, 0);          /* 绿灯常亮 */
    } else {
        key2_override = 0;
        RGB_ModeSet(current_mode);        /* 恢复之前的模式 */
    }
}
