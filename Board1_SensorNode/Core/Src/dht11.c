/**
 * @file    dht11.c
 * @brief   DHT11 温湿度传感器驱动实现
 * @note    使用微秒级延时 (SysTick 或 DWT)，读取期间需关闭调度器
 *          在 FreeRTOS 中调用时需要用 vTaskSuspendAll() 保护时序
 */

#include "dht11.h"
#include "FreeRTOS.h"
#include "task.h"

/* ==================== 内部宏 ==================== */

/* 快速 GPIO 操作 */
#define DHT11_DQ_HIGH()   HAL_GPIO_WritePin(DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_PIN_SET)
#define DHT11_DQ_LOW()    HAL_GPIO_WritePin(DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_PIN_RESET)
#define DHT11_DQ_READ()   HAL_GPIO_ReadPin(DHT11_GPIO_PORT, DHT11_GPIO_PIN)

/* ==================== 微秒延时 (DWT 实现) ==================== */

/**
 * @brief  初始化 DWT 计数器用于微秒延时
 * @note   需要在 main 函数初始化阶段调用一次
 */
static void DWT_Init(void)
{
    /* 解锁 DWT 访问 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    /* 清零并启动计数器 */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  微秒级延时 (基于 CPU 周期计数器)
 * @param  us  微秒数
 * @note   系统时钟 = 72MHz，每微秒 = 72 个周期
 */
static void DWT_Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

/* ==================== GPIO 模式切换 ==================== */

static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  /* 开漏输出 */
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;          /* 上拉输入 */
    HAL_GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

/* ==================== API 实现 ==================== */

void DHT11_Init(void)
{
    DWT_Init();                /* 初始化微秒延时 */
    DHT11_SetOutput();
    DHT11_DQ_HIGH();           /* 总线空闲高电平 */
}

uint8_t DHT11_Read(DHT11_Data_t *data)
{
    uint8_t buf[5] = {0};      /* 40bit 数据缓冲区 */
    uint8_t i, j;
    uint32_t timeout;

    if (data == NULL) return 1;

    /* ---- 在 FreeRTOS 中保护时序 ---- */
    vTaskSuspendAll();

    /* ---- 步骤 1: 主机发送起始信号 ---- */
    DHT11_SetOutput();
    DHT11_DQ_LOW();            /* 拉低 >18ms */
    DWT_Delay_us(20000);       /* 20ms */
    DHT11_DQ_HIGH();           /* 拉高等待响应 */
    DWT_Delay_us(30);          /* 30us */

    /* ---- 步骤 2: 切换到输入，等待 DHT11 响应 ---- */
    DHT11_SetInput();

    /* 等待 DHT11 拉低 (响应信号) */
    timeout = DHT11_TIMEOUT;
    while (DHT11_DQ_READ() == GPIO_PIN_SET) {
        if (--timeout == 0) {
            xTaskResumeAll();
            data->status = 1;  /* 超时 */
            return 1;
        }
        DWT_Delay_us(1);
    }

    /* 等待 DHT11 释放总线 (拉低 ~80us 后拉高 ~80us) */
    timeout = DHT11_TIMEOUT;
    while (DHT11_DQ_READ() == GPIO_PIN_RESET) {
        if (--timeout == 0) {
            xTaskResumeAll();
            data->status = 1;
            return 1;
        }
        DWT_Delay_us(1);
    }
    timeout = DHT11_TIMEOUT;
    while (DHT11_DQ_READ() == GPIO_PIN_SET) {
        if (--timeout == 0) {
            xTaskResumeAll();
            data->status = 1;
            return 1;
        }
        DWT_Delay_us(1);
    }

    /* ---- 步骤 3: 读取 40 bit 数据 ---- */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 8; j++) {
            /* 等待低电平结束 (每个 bit 从 50us 低电平开始) */
            timeout = DHT11_TIMEOUT;
            while (DHT11_DQ_READ() == GPIO_PIN_RESET) {
                if (--timeout == 0) {
                    xTaskResumeAll();
                    data->status = 1;
                    return 1;
                }
                DWT_Delay_us(1);
            }

            /* 高电平持续 26-28us = '0', 70us = '1' */
            DWT_Delay_us(40);  /* 延时到 40us，之后采样 */

            buf[i] <<= 1;
            if (DHT11_DQ_READ() == GPIO_PIN_SET) {
                buf[i] |= 0x01;  /* 高电平还在 → bit=1 */
            }

            /* 等待高电平结束 */
            timeout = DHT11_TIMEOUT;
            while (DHT11_DQ_READ() == GPIO_PIN_SET) {
                if (--timeout == 0) {
                    xTaskResumeAll();
                    data->status = 1;
                    return 1;
                }
                DWT_Delay_us(1);
            }
        }
    }

    xTaskResumeAll();          /* 恢复调度器 */

    /* ---- 步骤 4: 校验 ---- */
    uint8_t checksum = buf[0] + buf[1] + buf[2] + buf[3];
    if (checksum != buf[4]) {
        data->status = 2;      /* 校验错误 */
        return 2;
    }

    /* ---- 步骤 5: 填充数据 ---- */
    data->humidity    = buf[0];  /* DHT11 小数部分固定为 0 */
    data->temperature = buf[2];
    data->status      = 0;

    return 0;  /* 成功 */
}
