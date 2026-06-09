/**
 * @file    main.c
 * @brief   Board 2 — 显示监控节点 (FreeRTOS + CAN RX + OLED)
 *
 *          CubeMX 配置要点:
 *          - RCC: HSE 8MHz Crystal → PLL ×9 → SYSCLK 72MHz
 *          - USART1: PA9(TX)/PA10(RX), 115200-8-N-1
 *          - TIM2: CH1(PA5)/CH2(PA6)/CH3(PA7) PWM, 1kHz
 *          - I2C1: PB6(SCL)/PB7(SDA), 400kHz
 *          - CAN1: PA11(RX)/PA12(TX), 500kbps
 *          - FREERTOS: CMSIS_V2
 */

#include "main.h"
#include "cmsis_os.h"
#include "rgb_led.h"
#include "oled.h"
#include "can_protocol.h"
#include "can_receiver.h"
#include "system_status.h"
#include <stdio.h>
#include <string.h>

/* ==================== FreeRTOS 句柄 ==================== */

osThreadId_t TaskCANRXHandle;
osThreadId_t TaskOLEDHandle;
osThreadId_t TaskRGBHandle;

/* 二值信号量 — CAN RX 中断唤醒任务 */
osSemaphoreId_t CANSemaphore;

/* 事件组 */
osEventFlagsId_t DisplayEventFlags;
#define EVENT_DATA_UPDATED  0x0001
#define EVENT_STATUS_UPDATED 0x0002

/* ==================== printf 重定向 ==================== */

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/* ==================== CAN RX 中断回调 ==================== */

/**
 * @brief  HAL CAN FIFO0 消息挂起回调 (在中断上下文中执行)
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0,
                             &rx_header, rx_data) == HAL_OK) {
        CAN_Receiver_ProcessRx(&rx_header, rx_data);
    }

    /* 释放信号量，唤醒 Task_CAN_RX */
    osSemaphoreRelease(CANSemaphore);
}

/* ==================== 任务实现 ==================== */

/**
 * @brief  Task_CAN_RX — 等待 CAN 信号量, 判断通信状态
 */
void Task_CAN_RX(void *argument)
{
    CAN_Receiver_Init();
    CAN_Receiver_Start();

    printf("[CAN RX] Ready, Filter: 0x201 & 0x202\r\n");

    for (;;) {
        /* 等待 CAN 中断信号量 (阻塞) */
        if (osSemaphoreAcquire(CANSemaphore, pdMS_TO_TICKS(2000)) == osOK) {

            /* 喂 CAN 看门狗 */
            SystemStatus_FeedCAN();

            /* 通知 OLED 任务数据已更新 */
            osEventFlagsSet(DisplayEventFlags, EVENT_DATA_UPDATED);
        } else {
            /* 超时: 2 秒内没有收到 CAN 帧 → 可能是通信故障 */
            printf("[CAN RX] Timeout! No data received.\r\n");
            osEventFlagsSet(DisplayEventFlags, EVENT_STATUS_UPDATED);
        }
    }
}

/**
 * @brief  Task_OLED — 每 500ms 刷新 OLED 显示
 */
void Task_OLED(void *argument)
{
    char line1[22], line2[22], line3[22], line4[22];
    SystemStatus_t prev_status = SYSTEM_SAFE;

    OLED_Init();
    OLED_Clear();

    /* 启动画面 */
    OLED_ShowString(20, 1, "STM32 Signal");
    OLED_ShowString(14, 3, "Detection System");
    OLED_ShowString(26, 5, "Board 2: RX");
    vTaskDelay(pdMS_TO_TICKS(2000));
    OLED_Clear();

    for (;;) {
        /* 检查系统状态 */
        SystemStatus_Check();
        SystemStatus_t status = SystemStatus_Get();

        /* 格式化显示内容 */
        if (g_can_rx_data.sensor_updated) {
            CAN_SensorFrame_t *s = &g_can_rx_data.sensor;

            /* 第一行: 温度 */
            snprintf(line1, sizeof(line1), "Temp: %d.%d C",
                     s->temp_int, s->temp_dec);

            /* 第二行: 湿度 */
            snprintf(line2, sizeof(line2), "Humi: %d.%d %%",
                     s->humi_int, s->humi_dec);

            /* 第三行: ADC */
            snprintf(line3, sizeof(line3), "ADC:  %d (%.2fV)",
                     s->adc_value,
                     (float)s->adc_value / 4095.0f * 3.3f);

            g_can_rx_data.sensor_updated = 0;
        } else {
            snprintf(line1, sizeof(line1), "Waiting data...");
            line2[0] = '\0';
            line3[0] = '\0';
        }

        /* 第四行: 系统状态 */
        snprintf(line4, sizeof(line4), "[%s]", SystemStatus_GetString());

        /* OLED 刷新 (仅更新有变化的区域可以优化速度) */
        OLED_ShowString(0, 0, line1);
        OLED_ShowString(0, 1, "");
        OLED_ShowString(0, 2, line2);
        OLED_ShowString(0, 3, "");
        OLED_ShowString(0, 4, line3);
        OLED_ShowString(0, 5, "");

        /* 状态栏: 反色高亮 */
        if (status != prev_status) {
            OLED_ShowHighlight(7);  /* 状态变化时闪烁提示 */
            prev_status = status;
        }
        OLED_ShowString(0, 7, line4);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief  Task_RGB — 根据系统状态 + CAN 数据刷新 RGB
 */
void Task_RGB(void *argument)
{
    RGB_Init();
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) {
        SystemStatus_Check();

        /* 如果传感器状态帧有错误, 优先使用 */
        if (g_can_rx_data.status_updated) {
            SystemStatus_Set(
                (SystemStatus_t)g_can_rx_data.status.system_status,
                g_can_rx_data.status.error_code);
            g_can_rx_data.status_updated = 0;
        }

        RGB_SetStatus((uint8_t)SystemStatus_Get());
        vTaskDelay(pdMS_TO_TICKS(500));
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
    MX_I2C1_Init();
    MX_CAN_Init();

    printf("\r\n\r\n========================================\r\n");
    printf("  Board2: 显示监控节点\r\n");
    printf("  STM32F103C8T6  FreeRTOS  CAN 500kbps\r\n");
    printf("========================================\r\n");

    /* 系统状态初始化 */
    SystemStatus_Init();

    /* 创建信号量 */
    CANSemaphore = osSemaphoreNew(10, 0, NULL);  /* 最大 10, 初始 0 */

    /* 创建事件组 */
    DisplayEventFlags = osEventFlagsNew(NULL);

    /* 创建 FreeRTOS 任务 */
    const osThreadAttr_t task_attr_normal = {
        .name = "Task", .priority = osPriorityNormal,
        .stack_size = 512
    };
    const osThreadAttr_t task_attr_high = {
        .name = "Task", .priority = osPriorityAboveNormal,
        .stack_size = 512
    };
    const osThreadAttr_t task_attr_low = {
        .name = "Task", .priority = osPriorityLow,
        .stack_size = 256
    };

    /* Board2 — 3 任务 */
    TaskCANRXHandle = osThreadNew(Task_CAN_RX, NULL, &task_attr_high);
    TaskOLEDHandle  = osThreadNew(Task_OLED,  NULL, &task_attr_normal);
    TaskRGBHandle   = osThreadNew(Task_RGB,   NULL, &task_attr_low);

    /* 启动 FreeRTOS 调度器 */
    osKernelStart();

    while (1);
}
