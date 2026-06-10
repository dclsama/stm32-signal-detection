/**
 * @file    buzzer.c
 * @brief   蜂鸣器驱动实现
 */

#include "buzzer.h"

/* ==================== 内部变量 ==================== */
static BuzzerState_t current_state = BEEP_OFF;
static uint32_t buzzer_timer = 0;       /* 间断计时器 */
static uint8_t  buzzer_on = 0;          /* 当前输出状态 */

/* ==================== 内部函数 ==================== */

static void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
    buzzer_on = 1;
}

static void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET);
    buzzer_on = 0;
}

/* ==================== API ==================== */

void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BUZZER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStruct);

    Buzzer_Off();
    current_state = BEEP_OFF;
    buzzer_timer = 0;
}

void Buzzer_SetState(BuzzerState_t state)
{
    current_state = state;
    buzzer_timer = 0;

    switch (state) {
        case BEEP_ON:
            Buzzer_On();
            break;
        case BEEP_OFF:
            Buzzer_Off();
            break;
        case BEEP_INTERVAL:
            Buzzer_On();                 /* 先从响开始 */
            buzzer_on = 1;
            break;
    }
}

void Buzzer_CycleNext(void)
{
    BuzzerState_t next = (current_state + 1) % BEEP_COUNT;
    Buzzer_SetState(next);
}

BuzzerState_t Buzzer_GetState(void)
{
    return current_state;
}

void Buzzer_Tick(void)
{
    if (current_state != BEEP_INTERVAL) return;

    buzzer_timer += 20;  /* 每 20ms */

    if (buzzer_on) {
        /* 正在响 → 检查是否到时 */
        if (buzzer_timer >= BUZZER_ON_MS) {
            Buzzer_Off();
            buzzer_timer = 0;
        }
    } else {
        /* 正在停 → 检查是否到时 */
        if (buzzer_timer >= BUZZER_OFF_MS) {
            Buzzer_On();
            buzzer_timer = 0;
        }
    }
}
