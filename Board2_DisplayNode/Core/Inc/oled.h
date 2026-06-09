/**
 * @file    oled.h
 * @brief   SSD1306 OLED 0.96" I2C 驱动 (128×64)
 * @note    I2C1: SCL=PB6, SDA=PB7
 *          地址: 0x78 (SA0=0, 即 0x3C << 1)
 */

#ifndef __OLED_H__
#define __OLED_H__

#include "main.h"
#include <stdarg.h>

/* ==================== 配置宏 ==================== */
#define OLED_I2C_HANDLE       (&hi2c1)
#define OLED_I2C_ADDR         0x78     /* 7bit: 0x3C, 8bit: 0x78 */

#define OLED_WIDTH            128
#define OLED_HEIGHT           64
#define OLED_PAGES            8        /* HEIGHT/8 */

/* ==================== 字体大小 ==================== */
#define OLED_FONT_6X8         0        /* 6×8 ASCII */
#define OLED_FONT_8X16        1        /* 8×16 ASCII */
#define OLED_FONT_16X16       2        /* 16×16 中文 (需字库) */

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化 OLED (发送初始化命令序列)
 */
void OLED_Init(void);

/**
 * @brief  清屏 (全黑)
 */
void OLED_Clear(void);

/**
 * @brief  全屏填充
 * @param  pattern  填充字节 (0x00=黑, 0xFF=白)
 */
void OLED_Fill(uint8_t pattern);

/**
 * @brief  设置光标位置 (页寻址模式)
 * @param  page  页号 (0~7, 每页 8 像素高)
 * @param  col   列号 (0~127)
 */
void OLED_SetCursor(uint8_t page, uint8_t col);

/**
 * @brief  显示一个 6×8 ASCII 字符
 * @param  x  列坐标 (0~127)
 * @param  y  行坐标 (0~7, 页号)
 * @param  ch  ASCII 字符
 */
void OLED_ShowChar(uint8_t x, uint8_t y, char ch);

/**
 * @brief  显示字符串 (6×8 字体)
 * @param  x  列坐标
 * @param  y  行坐标 (页号)
 * @param  str 字符串
 */
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);

/**
 * @brief  显示数字
 * @param  x    列坐标
 * @param  y    行坐标 (页号)
 * @param  num  数字
 * @param  len  显示位数
 */
void OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len);

/**
 * @brief  显示浮点数
 * @param  x      列坐标
 * @param  y      行坐标
 * @param  num    浮点数
 * @param  int_len  整数位数
 * @param  dec_len  小数位数
 */
void OLED_ShowFloat(uint8_t x, uint8_t y, float num,
                    uint8_t int_len, uint8_t dec_len);

/**
 * @brief  OLED 格式化输出 (类似 printf)
 * @param  x    列坐标
 * @param  y    行坐标
 * @param  fmt  格式化字符串 (支持 %d, %s, %c, %f)
 */
void OLED_Printf(uint8_t x, uint8_t y, const char *fmt, ...);

/**
 * @brief  显示一行反色选中效果 (用于高亮状态)
 * @param  y  行号 (页号 0~7)
 */
void OLED_ShowHighlight(uint8_t y);

#endif /* __OLED_H__ */
