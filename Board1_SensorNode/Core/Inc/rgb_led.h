/**
 * @file    rgb_led.h
 * @brief   共阴 RGB LED PWM 控制
 * @note    TIM2 CH1 (PA5) = Red, CH2 (PA6) = Green, CH3 (PA7) = Blue
 *          PWM 频率 1kHz，占空比范围 0-999 (对应 0%-100%)
 */

#ifndef __RGB_LED_H__
#define __RGB_LED_H__

#include "main.h"

/* ==================== 配置宏 ==================== */
#define RGB_TIM_HANDLE        (&htim3)

#define RGB_RED_CHANNEL       TIM_CHANNEL_1   /* PA6  TIM3_CH1 */
#define RGB_GREEN_CHANNEL     TIM_CHANNEL_2   /* PA7  TIM3_CH2 */
#define RGB_BLUE_CHANNEL      TIM_CHANNEL_3   /* PB0  TIM3_CH3 */

#define PWM_PERIOD            999             /* ARR = 1000-1 */

/* ==================== 颜色预设 (0~999) ==================== */
#define RGB_OFF               {0, 0, 0}
#define RGB_RED               {999, 0, 0}
#define RGB_GREEN             {0, 999, 0}
#define RGB_BLUE              {0, 0, 999}
#define RGB_YELLOW            {999, 999, 0}
#define RGB_CYAN              {0, 999, 999}
#define RGB_MAGENTA           {999, 0, 999}
#define RGB_WHITE             {999, 999, 999}

/* ==================== 系统状态对应颜色 ==================== */
#define RGB_COLOR_SAFE        RGB_GREEN     /* SAFE    → 绿色 */
#define RGB_COLOR_ERROR1      RGB_YELLOW    /* ERROR1  → 黄色 传感器故障 */
#define RGB_COLOR_ERROR2      RGB_RED       /* ERROR2  → 红色 CAN通信故障 */

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化 RGB PWM (启动 TIM2 三通道)
 */
void RGB_Init(void);

/**
 * @brief  设置 RGB 颜色
 * @param  r  红色占空比 (0~999)
 * @param  g  绿色占空比 (0~999)
 * @param  b  蓝色占空比 (0~999)
 */
void RGB_SetColor(uint16_t r, uint16_t g, uint16_t b);

/**
 * @brief  根据系统状态设置 RGB 颜色
 * @param  status  0=SAFE, 1=ERROR1, 2=ERROR2
 */
void RGB_SetStatus(uint8_t status);

/**
 * @brief  关闭 RGB
 */
void RGB_Off(void);

#endif /* __RGB_LED_H__ */
