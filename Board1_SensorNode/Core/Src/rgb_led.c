/**
 * @file    rgb_led.c
 * @brief   共阴 RGB LED PWM 控制实现
 */

#include "rgb_led.h"

void RGB_Init(void)
{
    /* 启动 PWM 输出 */
    HAL_TIM_PWM_Start(RGB_TIM_HANDLE, RGB_RED_CHANNEL);
    HAL_TIM_PWM_Start(RGB_TIM_HANDLE, RGB_GREEN_CHANNEL);
    HAL_TIM_PWM_Start(RGB_TIM_HANDLE, RGB_BLUE_CHANNEL);

    /* 初始状态: 关闭 */
    RGB_Off();
}

void RGB_SetColor(uint16_t r, uint16_t g, uint16_t b)
{
    __HAL_TIM_SET_COMPARE(RGB_TIM_HANDLE, RGB_RED_CHANNEL,   r);
    __HAL_TIM_SET_COMPARE(RGB_TIM_HANDLE, RGB_GREEN_CHANNEL, g);
    __HAL_TIM_SET_COMPARE(RGB_TIM_HANDLE, RGB_BLUE_CHANNEL,  b);
}

void RGB_SetStatus(uint8_t status)
{
    switch (status) {
        case 0:  /* SAFE */
            RGB_SetColor(0, 999, 0);     /* 绿色 */
            break;
        case 1:  /* ERROR1 — 传感器故障 */
            RGB_SetColor(999, 999, 0);   /* 黄色 */
            break;
        case 2:  /* ERROR2 — CAN通信故障 */
            RGB_SetColor(999, 0, 0);     /* 红色 */
            break;
        default:
            RGB_Off();
            break;
    }
}

void RGB_Off(void)
{
    RGB_SetColor(0, 0, 0);
}
