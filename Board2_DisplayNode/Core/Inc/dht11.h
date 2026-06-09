/**
 * @file    dht11.h
 * @brief   DHT11 温湿度传感器驱动 (OneWire 协议)
 * @note    使用 GPIO 模拟时序，读取期间需关闭中断防止时序被打断
 *
 *          DHT11 数据格式 (40 bit):
 *          ┌─────────┬─────────┬─────────┬─────────┬─────────┐
 *          │ 湿度整数 │ 湿度小数 │ 温度整数 │ 温度小数 │ 校验和  │
 *          │  8bit   │  8bit   │  8bit   │  8bit   │  8bit   │
 *          └─────────┴─────────┴─────────┴─────────┴─────────┘
 *          校验和 = 前4字节之和的低8位 (DHT11小数部分固定为0)
 */

#ifndef __DHT11_H__
#define __DHT11_H__

#include "main.h"

/* ==================== 配置宏 ==================== */
/* 根据 CubeMX 引脚分配修改以下宏 */
#define DHT11_GPIO_PORT        GPIOA
#define DHT11_GPIO_PIN         GPIO_PIN_0

/* 超时阈值 (微秒级) */
#define DHT11_TIMEOUT          200

/* ==================== 数据结构 ==================== */

/** DHT11 传感器数据 */
typedef struct {
    uint8_t temperature;    /* 温度 整数部分 (°C) */
    uint8_t humidity;       /* 湿度 整数部分 (%RH) */
    uint8_t status;         /* 0=OK, 1=超时错误, 2=校验错误 */
} DHT11_Data_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化 DHT11 (配置 GPIO 为输出模式)
 */
void DHT11_Init(void);

/**
 * @brief  读取一次 DHT11 数据 (阻塞式，约20ms)
 * @param  data  数据指针
 * @return uint8_t  0=成功, 1=超时, 2=校验错误
 */
uint8_t DHT11_Read(DHT11_Data_t *data);

#endif /* __DHT11_H__ */
