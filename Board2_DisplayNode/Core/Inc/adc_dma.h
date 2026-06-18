#ifndef __ADC_DMA_H__
#define __ADC_DMA_H__
#include "main.h"
#define ADC_CHANNEL_COUNT     2
#define ADC_SAMPLE_PER_CH     16
#define ADC_BUF_SIZE          32
#define ADC_VREF              3.3f
typedef struct { uint16_t raw_value; float voltage; uint8_t status; } ADC_ChData_t;
typedef struct { ADC_ChData_t ch1; ADC_ChData_t ch2; } ADC_Data_t;
#endif
