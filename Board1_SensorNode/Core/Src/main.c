/**
 * @file    main.c
 * @brief   Board 1 — 传感器采集节点 (FreeRTOS + CAN TX)
 *
 *          CubeMX 生成的硬件初始化代码在 main.h / 自动生成的函数中。
 *          本文件包含 FreeRTOS 任务实现和用户应用逻辑。
 *
 *          CubeMX 配置要点:
 *          - RCC: HSE 8MHz Crystal → PLL ×9 → SYSCLK 72MHz
 *          - SYS: Debug Serial Wire, Timebase: TIM1 (FreeRTOS 使用 SysTick)
 *          - USART1: PA9(TX)/PA10(RX), 115200-8-N-1
 *          - TIM2: CH1(PA5)/CH2(PA6)/CH3(PA7) PWM, 1kHz
 *          - ADC1: IN1(PA1), 12bit, Continuous + DMA Circular
 *          - SPI2: PB13(SCK)/PB14(MISO)/PB15(MOSI), Mode0, 9MHz
 *          - CAN1: PA11(RX)/PA12(TX), 500kbps
 *          - FREERTOS: CMSIS_V2, 总堆大小 15KB
 */

#include "main.h"
#include "cmsis_os.h"
#include "dht11.h"
#include "rgb_led.h"
#include "adc_dma.h"
#include "w25q64.h"
#include "can_protocol.h"
#include "system_status.h"
#include <stdio.h>
#include <string.h>

/* ==================== FreeRTOS 句柄 ==================== */

/* 任务句柄 */
osThreadId_t TaskDHT11Handle;
osThreadId_t TaskADCHandle;
osThreadId_t TaskCANTXHandle;
osThreadId_t TaskRGBHandle;
osThreadId_t TaskFlashLogHandle;

/* 队列 — 传递传感器数据 */
osMessageQueueId_t SensorDataQueue;
#define SENSOR_QUEUE_SIZE   8

/* 事件组 — 系统状态同步 */
osEventFlagsId_t SystemEventFlags;
#define EVENT_SENSOR_OK     0x0001
#define EVENT_SENSOR_ERROR  0x0002
#define EVENT_CAN_TX_DONE   0x0004

/* 定时器 — Flash 日志写入 */
osTimerId_t FlashLogTimer;

/* ==================== 传感器数据结构 ==================== */

typedef struct {
    DHT11_Data_t dht11;
    ADC_Data_t   adc;
    TickType_t   tick;
} SensorData_t;

/* DMA 缓冲区 (ADC) — 在 CubeMX 生成的 adc.c 中声明 */
uint16_t adc_dma_buffer[ADC_SAMPLE_COUNT];

/* ==================== printf 重定向 ==================== */

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/* ==================== 任务实现 ==================== */

/**
 * @brief  Task_DHT11 — 每 2s 读取一次温湿度
 */
void Task_DHT11(void *argument)
{
    DHT11_Data_t dht11_data;

    DHT11_Init();
    vTaskDelay(pdMS_TO_TICKS(2000));  /* 上电后等 DHT11 稳定 */

    for (;;) {
        uint8_t ret = DHT11_Read(&dht11_data);

        if (ret == 0) {
            printf("[DHT11] T:%dC H:%d%%\r\n",
                   dht11_data.temperature, dht11_data.humidity);

            /* 喂传感器看门狗 */
            SystemStatus_FeedSensor();
            osEventFlagsSet(SystemEventFlags, EVENT_SENSOR_OK);
        } else {
            printf("[DHT11] Error: %d\r\n", ret);
            osEventFlagsSet(SystemEventFlags, EVENT_SENSOR_ERROR);
        }

        /* 将数据发送到队列 (非阻塞) */
        SensorData_t sensor_data;
        sensor_data.dht11 = dht11_data;
        sensor_data.tick  = xTaskGetTickCount();
        osMessageQueuePut(SensorDataQueue, &sensor_data, 0, 0);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief  Task_ADC — 每 100ms 读取 ADC (DMA 循环缓冲, 读最新值)
 */
void Task_ADC(void *argument)
{
    ADC_Data_t adc_data;

    ADC_DMA_Init();

    for (;;) {
        ADC_DMA_Read(&adc_data);
        /* ADC_DMA_Read 已经做均值滤波, 直接使用即可 */

        /* 更新到队列中最新一条 (与 DHT11 一起发送) */
        /* 这里不单独发送, 由 Task_CAN_TX 取队列时读取最新的 ADC */

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief  Task_CAN_TX — 每 500ms 打包 CAN 帧发送
 */
void Task_CAN_TX(void *argument)
{
    SensorData_t sensor_data;
    CAN_SensorFrame_t data_frame;
    CAN_StatusFrame_t status_frame;
    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t tx_mailbox;

    /* 配置 CAN TX 帧头 */
    tx_header.ExtId = 0;
    tx_header.IDE   = CAN_ID_STD;       /* 标准帧 */
    tx_header.RTR   = CAN_RTR_DATA;     /* 数据帧 */
    tx_header.TransmitGlobalTime = DISABLE;

    /* 等待其他任务初始化完成 */
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;) {
        /* 从队列获取最新传感器数据 (阻塞 100ms) */
        if (osMessageQueueGet(SensorDataQueue, &sensor_data, NULL,
                              pdMS_TO_TICKS(100)) == osOK) {

            /* 打包数据帧 */
            ADC_Data_t adc;
            ADC_DMA_Read(&adc);
            CAN_PackSensorFrame(&data_frame, &sensor_data.dht11, &adc);

            /* 发送数据帧 (CAN ID 0x201) */
            tx_header.StdId = CAN_ID_SENSOR_DATA;
            tx_header.DLC   = 8;
            if (HAL_CAN_AddTxMessage(&hcan, &tx_header,
                                     (uint8_t *)&data_frame,
                                     &tx_mailbox) == HAL_OK) {
                CAN_PrintSensorFrame(&data_frame);
            } else {
                printf("[CAN TX] Send Failed!\r\n");
            }

            /* 打包并发送状态帧 (CAN ID 0x202) */
            SystemStatus_Check();
            CAN_PackStatusFrame(&status_frame,
                                SystemStatus_Get(),
                                SystemStatus_GetErrorCode());

            tx_header.StdId = CAN_ID_STATUS;
            tx_header.DLC   = 2;
            HAL_CAN_AddTxMessage(&hcan, &tx_header,
                                 (uint8_t *)&status_frame,
                                 &tx_mailbox);

            osEventFlagsSet(SystemEventFlags, EVENT_CAN_TX_DONE);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief  Task_RGB — 每 500ms 根据系统状态刷新 RGB 颜色
 */
void Task_RGB(void *argument)
{
    RGB_Init();
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) {
        SystemStatus_Check();
        RGB_SetStatus((uint8_t)SystemStatus_Get());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief  Task_FlashLog — 定期将传感器数据写入 W25Q64 Flash
 * @note   由 FreeRTOS 软件定时器触发 (每 10 秒)
 */
void Task_FlashLog(void *argument)
{
    W25Q64_JEDEC_t jedec;
    FlashLogEntry_t entry;
    SensorData_t sensor_data;
    ADC_Data_t adc;

    W25Q64_Init();

    /* 读取 JEDEC ID 验证 */
    W25Q64_ReadJEDEC(&jedec);
    printf("[Flash] JEDEC ID: 0x%02X%02X%02X\r\n",
           jedec.manufacturer, jedec.memory_type, jedec.capacity);

    /* 检查是否是 W25Q64 (0xEF4017) */
    if (jedec.manufacturer != 0xEF ||
        jedec.memory_type  != 0x40 ||
        jedec.capacity     != 0x17) {
        printf("[Flash] WARNING: Unexpected JEDEC ID!\r\n");
    }

    /* 初始化日志系统 */
    W25Q64_LogInit();
    printf("[Flash] Log Init OK, Count: %lu\r\n",
           W25Q64_LogGetCount());

    for (;;) {
        /* 等待定时器通知 (或直接用 vTaskDelay) */
        vTaskDelay(pdMS_TO_TICKS(10000));  /* 10 秒 */

        /* 获取最新传感器数据 */
        if (osMessageQueueGet(SensorDataQueue, &sensor_data, NULL, 0) == osOK) {
            ADC_DMA_Read(&adc);

            entry.timestamp   = xTaskGetTickCount() / 1000; /* 秒 */
            entry.temperature = sensor_data.dht11.temperature;
            entry.humidity    = sensor_data.dht11.humidity;
            entry.adc_value   = adc.raw_value;
            entry.status      = sensor_data.dht11.status;

            W25Q64_LogAppend(&entry);
            printf("[Flash] Log saved, Total: %lu\r\n",
                   W25Q64_LogGetCount());
        }
    }
}

/* ==================== main() ==================== */

int main(void)
{
    /* CubeMX 生成的硬件初始化 */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_TIM2_Init();
    MX_ADC1_Init();
    MX_SPI2_Init();
    MX_CAN_Init();

    /* 启动 CAN */
    HAL_CAN_Start(&hcan);

    printf("\r\n\r\n========================================\r\n");
    printf("  Board1: 传感器采集节点\r\n");
    printf("  STM32F103C8T6  FreeRTOS  CAN 500kbps\r\n");
    printf("========================================\r\n");

    /* 系统状态初始化 */
    SystemStatus_Init();

    /* 创建队列 */
    SensorDataQueue = osMessageQueueNew(SENSOR_QUEUE_SIZE,
                                        sizeof(SensorData_t), NULL);

    /* 创建事件组 */
    SystemEventFlags = osEventFlagsNew(NULL);

    /* 创建 FreeRTOS 任务 */
    const osThreadAttr_t task_attr_normal = {
        .name = "Task", .priority = osPriorityNormal,
        .stack_size = 256
    };
    const osThreadAttr_t task_attr_high = {
        .name = "Task", .priority = osPriorityAboveNormal,
        .stack_size = 512
    };
    const osThreadAttr_t task_attr_low = {
        .name = "Task", .priority = osPriorityLow,
        .stack_size = 256
    };

    /* Board1 — 5 任务 */
    TaskDHT11Handle     = osThreadNew(Task_DHT11,     NULL, &task_attr_normal);
    TaskADCHandle       = osThreadNew(Task_ADC,       NULL, &task_attr_normal);
    TaskCANTXHandle     = osThreadNew(Task_CAN_TX,    NULL, &task_attr_high);
    TaskRGBHandle       = osThreadNew(Task_RGB,       NULL, &task_attr_low);
    TaskFlashLogHandle  = osThreadNew(Task_FlashLog,  NULL, &task_attr_low);

    /* 启动 FreeRTOS 调度器 (永不返回) */
    osKernelStart();

    /* 不应到达此处 */
    while (1);
}
