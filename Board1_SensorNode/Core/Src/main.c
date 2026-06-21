/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Board1 — 传感器采集节点  FreeRTOS + CAN TX
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dht11.h"
#include "rgb_led.h"
#include "adc_dma.h"
#include "w25q64.h"
#include "can_protocol.h"
#include "system_status.h"
#include "key.h"
#include "rgb_modes.h"
#include "threshold.h"
#include "can_command.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct {
    DHT11_Data_t dht11;
    ADC_Data_t   adc;
    TickType_t   tick;
} SensorData_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SENSOR_QUEUE_SIZE     8
#define EVENT_SENSOR_OK       0x0001
#define EVENT_SENSOR_ERROR    0x0002
#define EVENT_CAN_TX_DONE     0x0004

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

CAN_HandleTypeDef hcan;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* ADC DMA 循环缓冲区 (2通道 × 16采样 = 32) */
uint16_t adc_dma_buffer[ADC_BUF_SIZE];

/* FreeRTOS 对象 */
osMessageQueueId_t SensorDataQueue;
osEventFlagsId_t  SystemEventFlags;

/* 任务句柄 */
osThreadId_t TaskDHT11Handle;
osThreadId_t TaskADCHandle;
osThreadId_t TaskCANTXHandle;
osThreadId_t TaskRGBHandle;
osThreadId_t TaskFlashLogHandle;
osThreadId_t TaskKeyScanHandle;
osThreadId_t TaskCANCMDHandle;

/* CAN RX 信号量 — ISR 唤醒命令处理 */
osSemaphoreId_t CANCmdSemaphore;

/* Flash SPI 互斥锁 (保护 W25Q64 跨任务写入) */
osMutexId_t FlashMutex;

/* stack_size 单位=字节 (CMSIS_V2). printf 实测需要 ~120w, 分配 384w=1536B */
const osThreadAttr_t task_attr_normal = {
    .name = "Task", .priority = osPriorityNormal, .stack_size = 1536
};
const osThreadAttr_t task_attr_high = {
    .name = "Task", .priority = osPriorityAboveNormal, .stack_size = 1536
};
const osThreadAttr_t task_attr_low = {
    .name = "Task", .priority = osPriorityLow, .stack_size = 1024
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_CAN_Init(void);
static void MX_SPI2_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

void Task_DHT11(void *argument);
void Task_ADC(void *argument);
void Task_CAN_TX(void *argument);
void Task_RGB(void *argument);
void Task_FlashLog(void *argument);
void Task_KeyScan(void *argument);
void Task_CAN_CMD(void *argument);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ==================== CAN RX 中断回调 (仅搬运数据) ==================== */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0,
                             &rx_header, rx_data) == HAL_OK) {
        CAN_Command_ProcessRx(&rx_header, rx_data);
    }
    osSemaphoreRelease(CANCmdSemaphore);
}

/* ==================== printf 重定向 ==================== */

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/* ==================== 栈溢出钩子 ==================== */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("\r\n!!!! STACK OVERFLOW: %s !!!!\r\n", pcTaskName);
    __disable_irq();
    while (1);
}

/* ==================== 任务实现 ==================== */

void Task_DHT11(void *argument)
{
    DHT11_Data_t dht11_data;
    SensorData_t sensor_data;

    DHT11_Init();
    vTaskDelay(pdMS_TO_TICKS(2000));   /* 等 DHT11 稳定 */

    for (;;) {
        uint8_t ret = DHT11_Read(&dht11_data);

        SystemStatus_FeedSensor();  /* 无论成败都喂狗 */

        if (ret == 0) {
            printf("[DHT11] T:%dC H:%d%%\r\n",
                   dht11_data.temperature, dht11_data.humidity);
            osEventFlagsSet(SystemEventFlags, EVENT_SENSOR_OK);
        } else {
            printf("[DHT11] Error: %d\r\n", ret);
            osEventFlagsSet(SystemEventFlags, EVENT_SENSOR_ERROR);
        }

        /* 非阻塞发送到队列 */
        sensor_data.dht11 = dht11_data;
        sensor_data.tick  = xTaskGetTickCount();
        osMessageQueuePut(SensorDataQueue, &sensor_data, 0, 0);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Task_ADC(void *argument)
{
    ADC_Data_t adc_data;

    for (;;) {
        ADC_DMA_Read(&adc_data);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Task_CAN_TX(void *argument)
{
    SensorData_t sensor_data;
    CAN_SensorFrame_t data_frame;
    CAN_StatusFrame_t status_frame;
    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t tx_mailbox;

    tx_header.ExtId = 0;
    tx_header.IDE   = CAN_ID_STD;
    tx_header.RTR   = CAN_RTR_DATA;
    tx_header.TransmitGlobalTime = DISABLE;

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;) {
        if (osMessageQueueGet(SensorDataQueue, &sensor_data, NULL,
                              pdMS_TO_TICKS(100)) == osOK) {

            /* 打包数据帧 */
            ADC_Data_t adc;
            ADC_DMA_Read(&adc);
            CAN_PackSensorFrame(&data_frame, &sensor_data.dht11, &adc);

            /* 发送 0x201 数据帧 */
            tx_header.StdId = CAN_ID_SENSOR_DATA;
            tx_header.DLC   = 8;
            uint8_t tec = (CAN1->ESR >> 16) & 0xFF;
            if (HAL_CAN_AddTxMessage(&hcan, &tx_header,
                                     (uint8_t *)&data_frame,
                                     &tx_mailbox) == HAL_OK) {
                CAN_PrintSensorFrame(&data_frame);
                /* TEC 监测: 高=异常, 低=恢复 (均需 3 次确认) */
                static uint8_t bad_cnt = 0, good_cnt = 0;
                if (tec > 127) {
                    good_cnt = 0;
                    if (++bad_cnt >= 3)
                        SystemStatus_Set(SYSTEM_ERROR2, ERR_CAN_TEC_HIGH);
                } else {
                    bad_cnt = 0;
                    if (++good_cnt >= 3 &&
                        SystemStatus_GetErrorCode() == ERR_CAN_TEC_HIGH)
                        SystemStatus_Set(SYSTEM_SAFE, ERR_NONE);
                }
            } else {
                printf("[CAN TX] Mailbox Full! TEC=%u\r\n", tec);
            }

            osEventFlagsSet(SystemEventFlags, EVENT_CAN_TX_DONE);
        }

        /* 始终喂 CAN 看门狗 (队列空≠CAN故障, 防止误报) */
        SystemStatus_FeedCAN();

        /* 始终发送 0x202 状态帧 (Board2 心跳依据) */
        CAN_PackStatusFrame(&status_frame,
                            SystemStatus_Get(),
                            SystemStatus_GetErrorCode(),
                            Threshold_GetConfig()->temp_high);
        printf("[CAN TX] Status: %s ThrT:%dC\r\n",
               SystemStatus_GetString(),
               Threshold_GetConfig()->temp_high);
        tx_header.StdId = CAN_ID_STATUS;
        tx_header.DLC   = 4;
        HAL_CAN_AddTxMessage(&hcan, &tx_header,
                             (uint8_t *)&status_frame, &tx_mailbox);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void Task_RGB(void *argument)
{
    SensorData_t sensor_data;
    DHT11_Data_t dht11 = {0};           /* 初始化为 0 */
    ADC_Data_t adc = {0};
    uint8_t sensor_ready = 0;           /* 收到过数据才做阈值检查 */

    /* RGB 已在 main() 和 Task_KeyScan 中初始化, 此处不需要 */
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) {
        SystemStatus_Check();

        /* ==== 阈值检查 (KEY2 按住时跳过) ==== */
        if (osMessageQueueGet(SensorDataQueue, &sensor_data, NULL, 0) == osOK) {
            dht11 = sensor_data.dht11;
            sensor_ready = 1;
        }
        ADC_DMA_Read(&adc);

        /* KEY2 按住时: 强制 SAFE, 跳过阈值判断 */
        if (!RGB_IsKey2Override()) {
            if (sensor_ready && dht11.status == 0) {
                SystemStatus_t st = SystemStatus_Get();
                uint8_t is_can_timeout = (st == SYSTEM_ERROR2 &&
                              SystemStatus_GetErrorCode() == ERR_CAN_TIMEOUT);

                if (!is_can_timeout) {
                    ThresholdResult_t thr = Threshold_Check(dht11.temperature,
                                             adc.ch1.voltage, adc.ch2.voltage);
                    if (thr == THR_ERR_TEMP) {
                        SystemStatus_Set(SYSTEM_ERROR1, ERR_TEMP_HIGH);
                    } else if (thr == THR_ERR_VOLT) {
                        SystemStatus_Set(SYSTEM_ERROR2, ERR_VOLT_LOW);
                    } else {
                        if (st != SYSTEM_SAFE && st != SYSTEM_ERROR2) {
                            SystemStatus_Set(SYSTEM_SAFE, ERR_NONE);
                        }
                    }
                }
            }
        }

        /* ==== RGB 控制 (KEY2 按住时跳过, 由 RGB_ModeKey2Control 接管) ==== */
        if (!RGB_IsKey2Override()) {
            SystemStatus_t sys_status = SystemStatus_Get();

            if (sys_status == SYSTEM_ERROR1) {
                RGB_SetStatus(1);  /* 黄 */
            } else if (sys_status == SYSTEM_ERROR2) {
                RGB_SetStatus(2);  /* 红 */
            } else {
                RGB_ModeBreatheTick();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief  Task_KeyScan — 每 10ms 扫描按键 (软件消抖), 处理模式切换
 *         PB1短按: 绿→红→绿呼吸 (3档循环)
 *         PA2短按: 温度阈值 +1°C 并保存到 Flash
 *         PA2长按: 重置温度阈值为 30°C 并保存到 Flash + 清告警 + 绿灯常亮
 */
void Task_KeyScan(void *argument)
{
    Key_Init();
    RGB_Init();
    RGB_ModeSet(MODE_GREEN_SOLID);

    uint32_t tick = 0;
    DHT11_Data_t dht11 = {0};
    ADC_Data_t   adc = {0};
    uint8_t sensor_ok = 0;
    uint8_t key2_held = 0;   /* PB2 当前是否被按住 */

    printf("[KeyScan] Started\r\n");

    for (;;) {
        /* === 读取当前状态 (一次, 全程一致) === */
        SystemStatus_t st_now = SystemStatus_Get();
        uint8_t err_now = SystemStatus_GetErrorCode();
        uint8_t in_alarm = (st_now != SYSTEM_SAFE);

        /* === 按键扫描 === */
        KeyEvent_t ev = Key_Scan();
        switch (ev) {
            case KEY_EVENT_SHORT_1:
                if (!in_alarm) {
                    printf("[BTN] PB1 short → mode %d → ", RGB_ModeGet());
                    RGB_ModeCycleNext();
                    printf("%d\r\n", RGB_ModeGet());
                }
                break;

            case KEY_EVENT_SHORT_2:
                key2_held = 0;
                RGB_ModeKey2Control(0);
                Threshold_IncTemp();
                Threshold_SaveToFlash();
                printf("[BTN] KEY2 short → temp+1, saved: %d C\r\n",
                       Threshold_GetConfig()->temp_high);
                break;

            case KEY_EVENT_LONG_2:
                Threshold_ResetTemp();
                Threshold_SaveToFlash();
                printf("[BTN] KEY2 long → temp reset to 30 C, saved\r\n");
                printf("[BTN] BEFORE clear: %s\r\n", SystemStatus_GetString());
                Threshold_ClearAlarm();
                printf("[BTN] AFTER  clear: %s\r\n", SystemStatus_GetString());
                st_now = SystemStatus_Get();
                in_alarm = 0;
                {
                    CAN_TxHeaderTypeDef hdr = {0};
                    CAN_StatusFrame_t sf;
                    uint32_t mb;
                    hdr.StdId = CAN_ID_STATUS; hdr.DLC = 4;
                    hdr.IDE = CAN_ID_STD; hdr.RTR = CAN_RTR_DATA;
                    CAN_PackStatusFrame(&sf, SYSTEM_SAFE, ERR_NONE,
                                       Threshold_GetConfig()->temp_high);
                    if (HAL_CAN_AddTxMessage(&hcan, &hdr, (uint8_t*)&sf, &mb) != HAL_OK)
                        printf("[BTN] WARN: immediate tx failed!\r\n");
                    else
                        printf("[BTN] Immediate SAFE frame sent OK\r\n");
                }
                /* fall through */
            case KEY_EVENT_PRESS_2:
                if (!key2_held) {
                    printf("[BTN] PA2 hold → SAFE\r\n");
                    RGB_ModeKey2Control(1);
                    key2_held = 1;
                }
                break;

            case KEY_EVENT_RELEASE_2:
                printf("[BTN] PA2 release → OFF\r\n");
                RGB_ModeKey2Control(0);
                key2_held = 0;
                RGB_ModeSet(MODE_OFF);
                break;

            default:
                break;
        }

        /* === 每 500ms: 检查 + 阈值 (PA2 按住时全部跳过) === */
        tick++;
        if (tick % 50 == 0) {
            if (!key2_held) {
                SystemStatus_Check();
                if (osMessageQueueGet(SensorDataQueue, &dht11, NULL, 0) == osOK) {
                    if (dht11.status == 0) sensor_ok = 1;
                }
                ADC_DMA_Read(&adc);
                /* 电压检测不依赖 DHT11, 上电即可工作 */
                ThresholdResult_t thr = Threshold_Check(dht11.temperature,
                                         adc.ch1.voltage, adc.ch2.voltage);
                if (thr == THR_ERR_TEMP) {
                    SystemStatus_Set(SYSTEM_ERROR1, ERR_TEMP_HIGH);
                } else if (thr == THR_ERR_VOLT) {
                    SystemStatus_Set(SYSTEM_ERROR2, ERR_VOLT_LOW);
                } else if (SystemStatus_GetErrorCode() == ERR_TEMP_HIGH ||
                           SystemStatus_GetErrorCode() == ERR_VOLT_LOW) {
                    SystemStatus_Set(SYSTEM_SAFE, ERR_NONE);
                }
                st_now = SystemStatus_Get();    /* 刷新 */
                err_now = SystemStatus_GetErrorCode();
                in_alarm = (st_now != SYSTEM_SAFE);
            }
        }

        /* === 每 5s 打印 (含滤波对比) === */
        if (tick % 500 == 0) {
            uint32_t tsr = CAN1->TSR;
            uint32_t esr = CAN1->ESR;
            /* 单次原始采样 (无滤波, DMA 缓冲区第一个数据) */
            uint16_t raw_single = ADC_DMA_ReadRaw(0);
            float    raw_v      = ADC_ToVoltage(raw_single);
            /* 16-sample 均值滤波 (ADC_DMA_Read 输出) */
            float    mean_v     = adc.ch1.voltage;
            /* EMA 滤波 (对均值进一步平滑) */
            float    ema_v      = ADC_EMA_Filter(mean_v, 0);
            printf("[INFO] t=%lus mode=%d status=%s "
                   "T=%dC | "
                   "CH1 raw=%u.%02uV avg=%u.%02uV ema=%u.%02uV | "
                   "CAN TSR=%04lX TEC=%lu REC=%lu\r\n",
                   (uint32_t)(xTaskGetTickCount() / 1000),
                   RGB_ModeGet(), SystemStatus_GetString(),
                   dht11.temperature,
                   (unsigned int)raw_v,
                   (unsigned int)(raw_v * 100.0f) % 100,
                   (unsigned int)mean_v,
                   (unsigned int)(mean_v * 100.0f) % 100,
                   (unsigned int)ema_v,
                   (unsigned int)(ema_v * 100.0f) % 100,
                   (unsigned long)(tsr & 0xFFFF),
                   (unsigned long)((esr >> 16) & 0xFF),
                   (unsigned long)((esr >> 24) & 0xFF));
        }

        /* === RGB 控制 (PA2 按住时绿灯优先) === */
        static uint8_t was_alarm = 0;
        if (key2_held) {
            was_alarm = 0;
        } else if (in_alarm) {
            was_alarm = 1;
            if (st_now == SYSTEM_ERROR1)
                RGB_SetColor(999, 0, 0);
            else if (err_now == ERR_CAN_TIMEOUT || err_now == ERR_CAN_TEC_HIGH)
                RGB_SetColor(0, 0, 999);
            else
                RGB_AlarmRedBreatheTick();
        } else {
            if (was_alarm) {
                RGB_ModeSet(RGB_ModeGet());  /* 刚从告警恢复: 立即恢复模式色 */
                was_alarm = 0;
            }
            RGB_ModeBreatheTick();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief  Task_CAN_CMD — 等待 0x301 命令帧, 清告警并回复
 */
void Task_CAN_CMD(void *argument)
{
    CAN_Command_Init();
    CAN_Command_Start();

    printf("[CAN CMD] Ready, Filter: 0x301\r\n");

    for (;;) {
        /* 等待 CAN RX 中断信号量 */
        if (osSemaphoreAcquire(CANCmdSemaphore, osWaitForever) == osOK) {
            CAN_Command_ProcessTask();  /* 任务中处理: 清告警 + 回复 */
        }
    }
}

void Task_FlashLog(void *argument)
{
    W25Q64_JEDEC_t jedec;
    FlashLogEntry_t entry;
    SensorData_t sensor_data;
    ADC_Data_t adc;
    ThresholdConfig_t *thr_cfg;

    W25Q64_Init();

    /* 读取并打印 JEDEC ID */
    W25Q64_ReadJEDEC(&jedec);
    printf("[Flash] JEDEC ID: 0x%02X%02X%02X\r\n",
           jedec.manufacturer, jedec.memory_type, jedec.capacity);

    if (jedec.manufacturer != 0xEF ||
        jedec.memory_type  != 0x40 ||
        jedec.capacity     != 0x17) {
        printf("[Flash] WARNING: Unexpected JEDEC!\r\n");
    }

    /* 从 Flash 加载阈值 (必须在 W25Q64_Init 之后) */
    Threshold_LoadFromFlash();

    /* 打印当前阈值 */
    thr_cfg = Threshold_GetConfig();
    printf("[Threshold] Current: temp_high=%d C, volt_low=%d.%02dV, enabled=%d\r\n",
           thr_cfg->temp_high, thr_cfg->volt_low / 100,
           thr_cfg->volt_low % 100, thr_cfg->enabled);

    W25Q64_LogInit();
    printf("[Flash] Log OK, Count: %lu\r\n", W25Q64_LogGetCount());

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        if (osMessageQueueGet(SensorDataQueue, &sensor_data, NULL, 0) == osOK) {
            ADC_DMA_Read(&adc);
            entry.timestamp   = xTaskGetTickCount() / 1000;
            entry.temperature = sensor_data.dht11.temperature;
            entry.humidity    = sensor_data.dht11.humidity;
            entry.adc_value   = adc.ch1.raw_value;
            entry.status      = sensor_data.dht11.status;

            /* Flash 写入加锁 (与阈值保存互斥) */
            if (osMutexAcquire(FlashMutex, pdMS_TO_TICKS(100)) == osOK) {
                W25Q64_LogAppend(&entry);
                osMutexRelease(FlashMutex);
                printf("[Flash] Saved, Total: %lu\r\n",
                       W25Q64_LogGetCount());
            }
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  ADC_DMA_Init();           /* 在调度器启动前开始 DMA */
  ADC_EMA_Init();           /* EMA 滤波器初始化 */
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_CAN_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  /* 外设初始化 */
  SystemStatus_Init();
  Key_Init();           /* 按键 PB1/PB2, 输入上拉 */
  RGB_Init();           /* RGB TIM3 PWM */
  RGB_ModeSet(MODE_GREEN_SOLID);
  HAL_CAN_Start(&hcan);
  CAN_Command_Init();  /* CAN RX 过滤器 (0x301) */
  CAN_Command_Start(); /* CAN RX 中断使能 */

  /* 禁用 printf 缓冲 — 确保数据立即发送 */
  setvbuf(stdout, NULL, _IONBF, 0);

  /* 禁用 printf 缓冲 */
  setvbuf(stdout, NULL, _IONBF, 0);

  printf("\r\n========================================\r\n");
  printf("  Board1: Sensor Node\r\n");
  printf("  STM32F103C8T6  FreeRTOS  CAN 500kbps\r\n");
  printf("========================================\r\n");

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */

  CANCmdSemaphore = osSemaphoreNew(10, 0, NULL);

  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */

  SensorDataQueue = osMessageQueueNew(SENSOR_QUEUE_SIZE,
                                      sizeof(SensorData_t), NULL);

  FlashMutex = osMutexNew(NULL);

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  /* 全部任务恢复 */
  TaskDHT11Handle    = osThreadNew(Task_DHT11,    NULL, &task_attr_normal);
  TaskADCHandle      = osThreadNew(Task_ADC,      NULL, &task_attr_normal);
  TaskCANTXHandle    = osThreadNew(Task_CAN_TX,   NULL, &task_attr_high);
  TaskCANCMDHandle   = osThreadNew(Task_CAN_CMD,  NULL, &task_attr_high);
  TaskKeyScanHandle  = osThreadNew(Task_KeyScan,  NULL, &task_attr_high);
  TaskRGBHandle      = osThreadNew(Task_RGB,      NULL, &task_attr_normal);
  TaskFlashLogHandle = osThreadNew(Task_FlashLog, NULL, &task_attr_low);

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  SystemEventFlags = osEventFlagsNew(NULL);

  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;       /* 扫描模式: CH1→CH3 */
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;                  /* 2 通道 */
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** CH1: PA1 = ADC_IN1 */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** CH2: PA3 = ADC_IN3 */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 9;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_5TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA2 (按键2) */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA3 (ADC_IN3 模拟输入) */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* PA4 = W25Q64 CS (软件 NSS, 输出推挽, 默认高 = 未选中) */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
