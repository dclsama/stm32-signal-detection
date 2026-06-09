/**
 * @file    w25q64.h
 * @brief   W25Q64 Flash SPI 驱动
 * @note    SPI2: SCK=PB13, MISO=PB14, MOSI=PB15, CS=PA4 (软件NSS)
 *          W25Q64 = 8MB (128 Block × 64KB, 32768 Sector × 4KB)
 *          JEDEC ID: 0xEF4017
 */

#ifndef __W25Q64_H__
#define __W25Q64_H__

#include "main.h"

/* ==================== 配置宏 ==================== */
#define W25Q64_SPI_HANDLE     (&hspi2)

#define W25Q64_CS_PORT        GPIOA
#define W25Q64_CS_PIN         GPIO_PIN_4

/* Flash 几何参数 */
#define W25Q64_PAGE_SIZE      256         /* 每页 256 字节 */
#define W25Q64_SECTOR_SIZE    4096        /* 每扇区 4KB (16页) */
#define W25Q64_BLOCK_SIZE     65536       /* 每块 64KB (16扇区) */
#define W25Q64_CAPACITY       8388608     /* 总容量 8MB */

/* 指令表 */
#define W25Q64_CMD_WRITE_ENABLE    0x06   /* 写使能 */
#define W25Q64_CMD_WRITE_DISABLE   0x04   /* 写禁能 */
#define W25Q64_CMD_READ_STATUS1    0x05   /* 读状态寄存器1 */
#define W25Q64_CMD_READ_STATUS2    0x35   /* 读状态寄存器2 */
#define W25Q64_CMD_READ_DATA       0x03   /* 读数据 */
#define W25Q64_CMD_FAST_READ       0x0B   /* 快速读 */
#define W25Q64_CMD_PAGE_PROGRAM    0x02   /* 页编程 (最多256字节) */
#define W25Q64_CMD_SECTOR_ERASE    0x20   /* 扇区擦除 (4KB) */
#define W25Q64_CMD_BLOCK_ERASE_32  0x52   /* 32KB块擦除 */
#define W25Q64_CMD_BLOCK_ERASE_64  0xD8   /* 64KB块擦除 */
#define W25Q64_CMD_CHIP_ERASE      0xC7   /* 整片擦除 (慢!) */
#define W25Q64_CMD_POWER_DOWN      0xB9   /* 掉电模式 */
#define W25Q64_CMD_RELEASE_PD      0xAB   /* 唤醒 */
#define W25Q64_CMD_DEVICE_ID       0x90   /* 设备ID (已废弃,用JEDEC) */
#define W25Q64_CMD_JEDEC_ID        0x9F   /* JEDEC ID */
#define W25Q64_CMD_READ_UID        0x4B   /* 唯一ID */

/* ==================== 数据结构 ==================== */

/** JEDEC ID 信息 */
typedef struct {
    uint8_t manufacturer;      /* 制造商 ID (Winbond=0xEF) */
    uint8_t memory_type;       /* 存储器类型 (W25Q64=0x40) */
    uint8_t capacity;          /* 容量代码 (W25Q64=0x17) */
} W25Q64_JEDEC_t;

/** Flash 日志记录 (10 字节) */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;        /* 时间戳 (秒, 相对系统启动) */
    uint8_t  temperature;      /* 温度 °C */
    uint8_t  humidity;         /* 湿度 %RH */
    uint16_t adc_value;        /* ADC 原始值 */
    uint8_t  status;           /* 传感器状态 */
    uint8_t  reserved;         /* 保留 */
} FlashLogEntry_t;

/* ==================== API 函数 ==================== */

/**
 * @brief  初始化 W25Q64 (SPI 已在 CubeMX 初始化)
 */
void W25Q64_Init(void);

/**
 * @brief  读取 JEDEC ID
 * @param  jedec  JEDEC 结构体指针
 */
void W25Q64_ReadJEDEC(W25Q64_JEDEC_t *jedec);

/**
 * @brief  读取状态寄存器 1
 * @return SR1 的值 (bit0=BUSY)
 */
uint8_t W25Q64_ReadStatus1(void);

/**
 * @brief  等待 Flash 空闲 (BUSY 位清零)
 */
void W25Q64_WaitBusy(void);

/**
 * @brief  扇区擦除 (4KB)
 * @param  sector_addr  扇区起始地址 (4KB 对齐)
 */
void W25Q64_SectorErase(uint32_t sector_addr);

/**
 * @brief  页写入 (最多 256 字节, 不能跨页!)
 * @param  addr  写入地址 (页内任意)
 * @param  data  数据指针
 * @param  len   数据长度 (≤256, 不能跨页边界)
 */
void W25Q64_PageWrite(uint32_t addr, uint8_t *data, uint16_t len);

/**
 * @brief  读取数据
 * @param  addr  起始地址
 * @param  data  接收缓冲区
 * @param  len   读取长度
 */
void W25Q64_ReadData(uint32_t addr, uint8_t *data, uint32_t len);

/**
 * @brief  写使能 (擦除/编程前必须调用)
 */
void W25Q64_WriteEnable(void);

/* ==================== 日志专用 API ==================== */

/**
 * @brief  初始化日志存储区域 (擦除 Sector 4)
 */
void W25Q64_LogInit(void);

/**
 * @brief  追加一条日志记录
 * @param  entry  日志条目指针
 * @note   循环覆盖: 写满后自动回到 Sector 4 开头
 */
void W25Q64_LogAppend(FlashLogEntry_t *entry);

/**
 * @brief  读取最近的 N 条日志
 * @param  entries  输出缓冲区
 * @param  max_count  最多读取条数
 * @param  actual_count  实际读取条数 (输出)
 */
void W25Q64_LogReadRecent(FlashLogEntry_t *entries, uint16_t max_count,
                          uint16_t *actual_count);

/**
 * @brief  获取当前日志计数
 * @return 已记录的日志条数
 */
uint32_t W25Q64_LogGetCount(void);

#endif /* __W25Q64_H__ */
