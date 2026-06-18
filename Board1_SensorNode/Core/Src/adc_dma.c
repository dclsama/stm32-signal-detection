/**
 * @file    adc_dma.c
 * @brief   ADC1 + DMA — 2路扫描 (PA1/PA3) + 均值滤波 + EMA 滤波
 * @note    选做一: 信号滤波
 *          - 原始值: DMA 缓冲区第一个采样点 (无滤波)
 *          - 均值滤波: 16-sample 算术平均 (ADC_DMA_Read)
 *          - EMA 滤波: 指数移动平均, 系数 α=0.15 (ADC_EMA_Filter)
 *          串口打印三者对比, 观察噪声抑制效果
 */

#include "adc_dma.h"

extern uint16_t adc_dma_buffer[ADC_BUF_SIZE];

static uint8_t adc_ready = 0;

/* EMA 滤波器状态 */
static float ema_ch1 = 0.0f;
static float ema_ch2 = 0.0f;
static uint8_t ema_inited = 0;

void ADC_DMA_Init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buffer, ADC_BUF_SIZE);
    adc_ready = 1;
}

/**
 * @brief  读取单个原始 ADC 采样值 (取 DMA 缓冲区第一位, 不做任何滤波)
 * @param  ch  0=CH1(PA1), 1=CH2(PA3)
 * @return 原始 ADC 值 (0~4095)
 */
uint16_t ADC_DMA_ReadRaw(uint8_t ch)
{
    if (!adc_ready) return 0;

    /* DMA buffer 交错排列: [CH1, CH2, CH1, CH2, ...]
     * ch=0 取 adc_dma_buffer[0], ch=1 取 adc_dma_buffer[1]
     */
    return adc_dma_buffer[ch];
}

void ADC_DMA_Read(ADC_Data_t *data)
{
    if (!adc_ready || data == NULL) return;

    /* 去交错: DMA buffer = [CH1,CH2, CH1,CH2, ...] */
    uint32_t sum1 = 0, sum2 = 0;
    for (uint8_t i = 0; i < ADC_BUF_SIZE; i += 2) {
        sum1 += adc_dma_buffer[i];       /* CH1: PA1 */
        sum2 += adc_dma_buffer[i + 1];   /* CH2: PA3 */
    }

    uint16_t avg1 = (uint16_t)(sum1 / ADC_SAMPLE_PER_CH);
    uint16_t avg2 = (uint16_t)(sum2 / ADC_SAMPLE_PER_CH);

    data->ch1.raw_value = avg1;
    data->ch1.voltage   = ADC_ToVoltage(avg1);
    data->ch1.status    = 0;

    data->ch2.raw_value = avg2;
    data->ch2.voltage   = ADC_ToVoltage(avg2);
    data->ch2.status    = 0;
}

/**
 * @brief  初始化 EMA 滤波器 (用当前 ADC 值作为初始值)
 */
void ADC_EMA_Init(void)
{
    ADC_Data_t adc;
    ADC_DMA_Read(&adc);
    ema_ch1 = adc.ch1.voltage;
    ema_ch2 = adc.ch2.voltage;
    ema_inited = 1;
}

/**
 * @brief  EMA 滤波: filtered = α·raw + (1-α)·filtered_prev
 * @param  raw_voltage  当前 ADC_DMA_Read 输出的电压 (16-sample 均值)
 * @param  ch           通道索引: 0=CH1, 1=CH2
 * @return EMA 滤波后的电压
 */
float ADC_EMA_Filter(float raw_voltage, uint8_t ch)
{
    if (!ema_inited) {
        ADC_EMA_Init();
    }

    float *ema = (ch == 0) ? &ema_ch1 : &ema_ch2;
    *ema = ADC_EMA_ALPHA * raw_voltage + (1.0f - ADC_EMA_ALPHA) * (*ema);
    return *ema;
}

float ADC_ToVoltage(uint16_t raw)
{
    return (float)raw / 4095.0f * ADC_VREF;
}
