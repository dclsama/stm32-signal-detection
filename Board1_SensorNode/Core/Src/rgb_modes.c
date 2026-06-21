/**
 * @file    rgb_modes.c
 * @brief   RGB LED 多模式控制器实现 (无浮点, 三角波呼吸)
 */

#include "rgb_modes.h"

/* ==================== 内部变量 ==================== */
static RGB_Mode_t current_mode = MODE_GREEN_SOLID;

/* 呼吸灯参数 — 三角波上升2000ms, 下降2000ms */
#define BREATHE_STEPS      50    /* 50级渐变, 每级 40ms */
static uint32_t breathe_tick = 0;
static int16_t  breathe_step = 0;
static int8_t   breathe_dir  = 1;  /* 1=增亮 -1=减弱 */

/* 按键2 覆盖标志 */
static uint8_t key2_override = 0;

/* 呼吸 2 秒一周期 */
#define BREATHE_STEPS        40

/* 查表: 三角波, 从 50→999→50 */
static const uint16_t breathe_table[BREATHE_STEPS] = {
    50,  74, 102, 135, 172, 213, 258, 306, 358, 413,
   471, 531, 592, 654, 715, 774, 830, 882, 928, 967,
   998, 998, 967, 928, 882, 830, 774, 715, 654, 592,
   531, 471, 413, 358, 306, 258, 213, 172, 135, 102
};

/* ==================== API ==================== */

void RGB_ModeSet(RGB_Mode_t mode)
{
    current_mode = mode;

    switch (mode) {
        case MODE_GREEN_SOLID:
            RGB_SetColor(0, 999, 0);
            break;
        case MODE_RED_SOLID:
            RGB_SetColor(999, 0, 0);
            break;
        case MODE_GREEN_BREATHE:
            breathe_tick = 0;
            breathe_step = 0;
            breathe_dir  = 1;
            break;
        case MODE_OFF:
        default:
            RGB_Off();
            break;
    }
}

void RGB_ModeCycleNext(void)
{
    if (key2_override) return;

    /* 循环 3 个显示模式: 绿常亮 → 红常亮 → 绿呼吸 → 绿常亮...
     * MODE_OFF 不在循环内, 但从 OFF 开始也回到 GREEN_SOLID */
    RGB_Mode_t next;
    switch (current_mode) {
        case MODE_GREEN_SOLID:   next = MODE_RED_SOLID;     break;
        case MODE_RED_SOLID:     next = MODE_GREEN_BREATHE; break;
        case MODE_GREEN_BREATHE: next = MODE_GREEN_SOLID;   break;
        default:                 next = MODE_GREEN_SOLID;   break;  /* MODE_OFF → 绿 */
    }
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

    breathe_tick++;
    if (breathe_tick < 2) return;     /* 每 20ms×2 = 40ms 一级 */

    breathe_tick = 0;
    breathe_step += breathe_dir;

    if (breathe_step >= BREATHE_STEPS - 1) {
        breathe_dir = -1;              /* 最亮 → 开始变暗 */
    } else if (breathe_step <= 0) {
        breathe_dir = 1;               /* 最暗 → 开始变亮 */
    }

    uint16_t g = breathe_table[breathe_step];
    RGB_SetColor(0, g, 0);             /* 绿色呼吸 (三角波近似) */
}

void RGB_ModeKey2Control(uint8_t pressed)
{
    if (pressed) {
        key2_override = 1;
        RGB_SetColor(0, 999, 0);
    } else {
        key2_override = 0;
        RGB_ModeSet(current_mode);
    }
}

uint8_t RGB_IsKey2Override(void)
{
    return key2_override;
}

/* ==================== 告警红色呼吸灯 ==================== */
static uint32_t alarm_breathe_tick = 0;
static int16_t  alarm_breathe_step = 0;
static int8_t   alarm_breathe_dir  = 1;

void RGB_AlarmRedBreatheTick(void)
{
    alarm_breathe_tick++;
    if (alarm_breathe_tick < 2) return;   /* 每 20ms×2=40ms 一级 */

    alarm_breathe_tick = 0;
    alarm_breathe_step += alarm_breathe_dir;

    if (alarm_breathe_step >= BREATHE_STEPS - 1) {
        alarm_breathe_dir = -1;
    } else if (alarm_breathe_step <= 0) {
        alarm_breathe_dir = 1;
    }

    uint16_t r = breathe_table[alarm_breathe_step];
    RGB_SetColor(r, 0, 0);             /* 红色呼吸 */
}
