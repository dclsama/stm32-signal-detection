/**
 * @file    adc_dma.h
 * @brief   ADC1 + DMA 驱动 — 2路电位器模拟信号采集
 * @note    CH1: PA1 (ADC1_IN1), CH2: PA3 (ADC1_IN3)
 *          12bit 分辨率, 扫描模式 + DMA 循环模式
 */

#ifndef __ADC_DMA_H__
#define __ADC_DMA_H__

#include "main.h"

/* ==================== 配置宏 ==================== */
#define ADC_CHANNEL_COUNT     2           /* 扫描 2 通道 */
#define ADC_SAMPLE_PER_CH     16          /* 每通道采样数 */
#define ADC_BUF_SIZE          (ADC_CHANNEL_COUNT * ADC_SAMPLE_PER_CH)  /* 32 */
#define ADC_VREF              3.3f        /* 参考电压 (V) */
#define ADC_EMA_ALPHA          0.15f       /* EMA 滤波系数 (0~1, 越小越平滑) */

/* ==================== 数据结构 ==================== */

/** 单通道 ADC 采集结果 */
typedef struct {
    uint16_t raw_value;        /* 原始 ADC 值 (0~4095) */
    float    voltage;          /* 换算电压 (V) */
    uint8_t  status;           /* 0=OK */
} ADC_ChData_t;

/** 双通道 ADC 采集结果 */
typedef struct {
    ADC_ChData_t ch1;          /* PA1: 电位器1 */
    ADC_ChData_t ch2;          /* PA3: 电位器2 */
} ADC_Data_t;

/* ==================== API 函数 ==================== */

void ADC_DMA_Init(void);

/**
 * @brief  读取单个原始采样值 (不做均值,用于对比滤波效果)
 * @param  ch 通道索引: 0=CH1(PA1), 1=CH2(PA3)
 * @return 原始 ADC 值 (0~4095)
 */
uint16_t ADC_DMA_ReadRaw(uint8_t ch);

/**
 * @brief  读取 2 路 ADC 数据 (16-sample 均值滤波, 去交错)
 */
void ADC_DMA_Read(ADC_Data_t *data);

/**
 * @brief  初始化 EMA 滤波器状态
 */
void ADC_EMA_Init(void);

/**
 * @brief  指数移动平均 (EMA) 滤波 — 对均值滤波后的电压进一步平滑
 * @param  raw_voltage  当前 ADC_DMA_Read 输出的电压值
 * @param  ch           通道索引: 0=CH1, 1=CH2
 * @return EMA 滤波后的电压值
 */
float ADC_EMA_Filter(float raw_voltage, uint8_t ch);

float ADC_ToVoltage(uint16_t raw);

#endif /* __ADC_DMA_H__ */
