/**
 * @file    adc_dma.c
 * @brief   ADC1 + DMA 驱动实现 — 循环采样 + 均值滤波
 */

#include "adc_dma.h"

/* DMA 循环缓冲区 (CubeMX 生成的 adc_dma_buffer 在 main.c 中) */
extern uint16_t adc_dma_buffer[ADC_SAMPLE_COUNT];

static uint8_t adc_ready = 0;      /* ADC+DMA 是否已启动 */

void ADC_DMA_Init(void)
{
    /* 校准 ADC */
    HAL_ADCEx_Calibration_Start(&hadc1);

    /* 启动 ADC+DMA (连续转换 + 循环模式) */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buffer, ADC_SAMPLE_COUNT);

    adc_ready = 1;
}

uint16_t ADC_DMA_Read(ADC_Data_t *data)
{
    uint32_t sum = 0;
    uint16_t avg;

    if (!adc_ready) {
        if (data) data->status = 1;
        return 0;
    }

    /* 对 DMA 循环缓冲区做均值滤波 */
    for (uint8_t i = 0; i < ADC_SAMPLE_COUNT; i++) {
        sum += adc_dma_buffer[i];
    }
    avg = (uint16_t)(sum / ADC_SAMPLE_COUNT);

    /* 填充结构化数据 */
    if (data != NULL) {
        data->raw_value = avg;
        data->voltage   = ADC_ToVoltage(avg);
        data->status    = 0;
    }

    return avg;
}

float ADC_ToVoltage(uint16_t raw)
{
    return (float)raw / 4095.0f * ADC_VREF;
}
