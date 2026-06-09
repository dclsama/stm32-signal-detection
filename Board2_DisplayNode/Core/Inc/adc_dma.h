/**
 * @file    adc_dma.h
 * @brief   ADC1 + DMA 驱动 — 电位器模拟信号采集
 * @note    ADC1_IN1 (PA1), 12bit 分辨率, 连续转换 + DMA 循环模式
 *          采集 16 次取均值滤波
 */

#ifndef __ADC_DMA_H__
#define __ADC_DMA_H__

#include "main.h"

/* ==================== 配置宏 ==================== */
#define ADC_SAMPLE_COUNT      16          /* DMA 缓冲区大小 */
#define ADC_VREF              3.3f        /* 参考电压 (V) */

/* ==================== 数据结构 ==================== */

/** ADC 采集结果 */
typedef struct {
    uint16_t raw_value;        /* 原始 ADC 值 (0~4095) */
    float    voltage;          /* 换算电压 (V) */
    uint8_t  status;           /* 0=OK */
} ADC_Data_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化 ADC1 + DMA (CubeMX 已配置硬件，此函数启动转换)
 */
void ADC_DMA_Init(void);

/**
 * @brief  读取 ADC 数据 (均值滤波)
 * @param  data  数据指针
 * @return 滤波后的 ADC 原始值
 */
uint16_t ADC_DMA_Read(ADC_Data_t *data);

/**
 * @brief  将 ADC 原始值转换为电压
 * @param  raw  ADC 原始值 (0~4095)
 * @return 电压值 (0~3.3V)
 */
float ADC_ToVoltage(uint16_t raw);

#endif /* __ADC_DMA_H__ */
