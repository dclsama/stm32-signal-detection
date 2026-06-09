/**
 * @file    w25q64.c
 * @brief   W25Q64 Flash SPI 驱动实现
 */

#include "w25q64.h"

/* ==================== CS 控制宏 ==================== */
#define W25Q64_CS_LOW()   HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_RESET)
#define W25Q64_CS_HIGH()  HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_SET)

/* ==================== SPI 收发 ==================== */

static uint8_t SPI_SendByte(uint8_t byte)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(W25Q64_SPI_HANDLE, &byte, &rx, 1, 100);
    return rx;
}

static void SPI_SendCmd(uint8_t cmd)
{
    W25Q64_CS_LOW();
    SPI_SendByte(cmd);
}

/* ==================== 基础操作 ==================== */

void W25Q64_Init(void)
{
    W25Q64_CS_HIGH();                    /* CS 初始高电平 */
    W25Q64_WriteEnable();                /* 确保可写 (读取ID不需要) */
}

void W25Q64_WriteEnable(void)
{
    W25Q64_CS_LOW();
    SPI_SendByte(W25Q64_CMD_WRITE_ENABLE);
    W25Q64_CS_HIGH();
}

void W25Q64_WaitBusy(void)
{
    uint8_t sr1;
    do {
        W25Q64_CS_LOW();
        SPI_SendByte(W25Q64_CMD_READ_STATUS1);
        sr1 = SPI_SendByte(0xFF);        /* 发送 dummy 读取 SR1 */
        W25Q64_CS_HIGH();
    } while (sr1 & 0x01);                /* BUSY bit */
}

uint8_t W25Q64_ReadStatus1(void)
{
    uint8_t sr1;
    W25Q64_CS_LOW();
    SPI_SendByte(W25Q64_CMD_READ_STATUS1);
    sr1 = SPI_SendByte(0xFF);
    W25Q64_CS_HIGH();
    return sr1;
}

/* ==================== JEDEC ID ==================== */

void W25Q64_ReadJEDEC(W25Q64_JEDEC_t *jedec)
{
    if (jedec == NULL) return;

    W25Q64_CS_LOW();
    SPI_SendByte(W25Q64_CMD_JEDEC_ID);
    jedec->manufacturer = SPI_SendByte(0xFF);  /* 期望 0xEF (Winbond) */
    jedec->memory_type  = SPI_SendByte(0xFF);  /* 期望 0x40 */
    jedec->capacity     = SPI_SendByte(0xFF);  /* 期望 0x17 (8MB) */
    W25Q64_CS_HIGH();
}

/* ==================== 地址发送 (24bit) ==================== */

static void SPI_SendAddr(uint32_t addr)
{
    SPI_SendByte((addr >> 16) & 0xFF);
    SPI_SendByte((addr >> 8)  & 0xFF);
    SPI_SendByte(addr & 0xFF);
}

/* ==================== 擦除操作 ==================== */

void W25Q64_SectorErase(uint32_t sector_addr)
{
    W25Q64_WriteEnable();
    W25Q64_WaitBusy();

    W25Q64_CS_LOW();
    SPI_SendByte(W25Q64_CMD_SECTOR_ERASE);
    SPI_SendAddr(sector_addr);
    W25Q64_CS_HIGH();

    W25Q64_WaitBusy();                   /* 扇区擦除约 45~400ms */
}

/* ==================== 读写操作 ==================== */

void W25Q64_PageWrite(uint32_t addr, uint8_t *data, uint16_t len)
{
    if (len == 0 || len > W25Q64_PAGE_SIZE) return;

    /* 检查不跨页 */
    uint16_t page_offset = addr % W25Q64_PAGE_SIZE;
    if (page_offset + len > W25Q64_PAGE_SIZE) {
        len = W25Q64_PAGE_SIZE - page_offset;  /* 截断到页边界 */
    }

    W25Q64_WriteEnable();
    W25Q64_WaitBusy();

    W25Q64_CS_LOW();
    SPI_SendByte(W25Q64_CMD_PAGE_PROGRAM);
    SPI_SendAddr(addr);
    for (uint16_t i = 0; i < len; i++) {
        SPI_SendByte(data[i]);
    }
    W25Q64_CS_HIGH();

    W25Q64_WaitBusy();                   /* 页编程约 0.7~3ms */
}

void W25Q64_ReadData(uint32_t addr, uint8_t *data, uint32_t len)
{
    W25Q64_CS_LOW();
    SPI_SendByte(W25Q64_CMD_READ_DATA);
    SPI_SendAddr(addr);
    for (uint32_t i = 0; i < len; i++) {
        data[i] = SPI_SendByte(0xFF);
    }
    W25Q64_CS_HIGH();
}

/* ==================== 日志系统 ==================== */

/* 日志存储区域: Sector 4 开始, 每个 Sector 可存约 409 条记录 */
#define LOG_START_SECTOR      4
#define LOG_START_ADDR        (LOG_START_SECTOR * W25Q64_SECTOR_SIZE)
#define LOG_MAX_SECTORS       2044      /* 剩余 Sector 数量 */
#define LOG_ENTRIES_PER_SECTOR (W25Q64_SECTOR_SIZE / sizeof(FlashLogEntry_t))

static uint32_t log_entry_count = 0;     /* 当前已写入日志条数 */

void W25Q64_LogInit(void)
{
    /* 读取 Sector 4 第一页, 获取日志计数 */
    W25Q64_ReadData(LOG_START_ADDR, (uint8_t *)&log_entry_count, 4);

    /* 如果计数非法, 初始化 */
    if (log_entry_count > 0xFFFFFFFF - 100) {
        log_entry_count = 0;
        W25Q64_SectorErase(LOG_START_ADDR);
    }
}

void W25Q64_LogAppend(FlashLogEntry_t *entry)
{
    uint32_t entry_offset = log_entry_count % LOG_ENTRIES_PER_SECTOR;
    uint32_t sector_offset = (log_entry_count / LOG_ENTRIES_PER_SECTOR) %
                              LOG_MAX_SECTORS;
    uint32_t addr = LOG_START_ADDR +
                    sector_offset * W25Q64_SECTOR_SIZE +
                    entry_offset * sizeof(FlashLogEntry_t);

    /* 如果是新扇区的开头, 先擦除 */
    if (entry_offset == 0) {
        W25Q64_SectorErase(LOG_START_ADDR +
                           sector_offset * W25Q64_SECTOR_SIZE);
    }

    /* 写入记录 */
    W25Q64_PageWrite(addr, (uint8_t *)entry, sizeof(FlashLogEntry_t));

    /* 更新计数 (写入 Sector 4 开头) */
    log_entry_count++;
    W25Q64_SectorErase(LOG_START_ADDR);
    uint8_t count_buf[4];
    count_buf[0] = log_entry_count & 0xFF;
    count_buf[1] = (log_entry_count >> 8)  & 0xFF;
    count_buf[2] = (log_entry_count >> 16) & 0xFF;
    count_buf[3] = (log_entry_count >> 24) & 0xFF;
    W25Q64_PageWrite(LOG_START_ADDR, count_buf, 4);
}

void W25Q64_LogReadRecent(FlashLogEntry_t *entries, uint16_t max_count,
                          uint16_t *actual_count)
{
    uint32_t total = log_entry_count;
    uint32_t start_idx = (total > max_count) ? (total - max_count) : 0;
    uint16_t count = (total > max_count) ? max_count : (uint16_t)total;

    for (uint16_t i = 0; i < count; i++) {
        uint32_t idx = start_idx + i;
        uint32_t entry_offset = idx % LOG_ENTRIES_PER_SECTOR;
        uint32_t sector_offset = (idx / LOG_ENTRIES_PER_SECTOR) %
                                  LOG_MAX_SECTORS;
        uint32_t addr = LOG_START_ADDR +
                        sector_offset * W25Q64_SECTOR_SIZE +
                        entry_offset * sizeof(FlashLogEntry_t);

        W25Q64_ReadData(addr, (uint8_t *)&entries[i],
                        sizeof(FlashLogEntry_t));
    }

    if (actual_count) *actual_count = count;
}

uint32_t W25Q64_LogGetCount(void)
{
    return log_entry_count;
}
