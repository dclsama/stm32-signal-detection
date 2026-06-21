/**
 * @file    threshold.h
 * @brief   阈值管理 — 温度/2路电压告警判断
 * @note    ERROR1: 温度 > 阈值 (30°C)
 *          ERROR2: 任何一路电压 < 阈值 (1.50V)
 */

#ifndef __THRESHOLD_H__
#define __THRESHOLD_H__

#include "main.h"

#define TEMP_THRESHOLD_DEFAULT      30    /* °C */
#define VOLT_THRESHOLD_DEFAULT     150    /* x100, 1.50V */

/* Flash 持久化存储 */
#define THRESHOLD_FLASH_SECTOR      0
#define THRESHOLD_FLASH_ADDR       (THRESHOLD_FLASH_SECTOR * 4096)  /* 0x000000 */
#define THRESHOLD_MAGIC             0x54485244UL   /* "THRD" ASCII */

/* 预设阈值 (KEY2 短按时写入) */
#define PRESET_TEMP_HIGH            40    /* 40°C */
#define PRESET_VOLT_LOW             180   /* 1.80V (x100) */

/** Flash 存储数据结构 (13 字节, packed) */
typedef struct __attribute__((packed)) {
    uint32_t magic;         /* 0x54485244 "THRD" */
    int16_t  temp_high;     /* 温度上限 °C */
    uint16_t volt_low;      /* 电压下限 x100 */
    uint8_t  enabled;       /* 使能标志 */
    uint8_t  reserved[3];   /* 填充 */
    uint8_t  checksum;      /* 字节 0~11 的 XOR */
} ThresholdFlashData_t;

typedef struct {
    int16_t  temp_high;
    uint16_t volt_low;
    uint8_t  enabled;
} ThresholdConfig_t;

typedef enum {
    THR_OK       = 0,
    THR_ERR_TEMP = 1,         /* → ERROR1 */
    THR_ERR_VOLT = 2,         /* → ERROR2 */
} ThresholdResult_t;

void Threshold_Init(void);
ThresholdResult_t Threshold_Check(int8_t temp_c, float volt1_v, float volt2_v);
ThresholdConfig_t* Threshold_GetConfig(void);
void Threshold_SetTemp(int16_t temp_c);
void Threshold_SetVolt(uint16_t volt_x100);
void Threshold_IncTemp(void);     /* 温度阈值 +1°C */
void Threshold_ResetTemp(void);   /* 重置温度阈值为默认 30°C */
void Threshold_ClearAlarm(void);

/**
 * @brief  从 W25Q64 Flash 加载阈值配置
 * @return 1 = 从 Flash 加载成功, 0 = 无有效数据 (使用默认值)
 * @note   需在 W25Q64_Init() 之后调用
 */
uint8_t Threshold_LoadFromFlash(void);

/**
 * @brief  保存当前阈值配置到 W25Q64 Flash (Sector 0)
 * @note   擦除 Sector 0 后写入, 含 magic + checksum 校验
 */
void Threshold_SaveToFlash(void);

#endif /* __THRESHOLD_H__ */
