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

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* ADC DMA 循环缓冲区 */
uint16_t adc_dma_buffer[ADC_SAMPLE_COUNT];

/* FreeRTOS 对象 */
osMessageQueueId_t SensorDataQueue;
osEventFlagsId_t  SystemEventFlags;

/* 任务句柄 */
osThreadId_t TaskDHT11Handle;
osThreadId_t TaskADCHandle;
osThreadId_t TaskCANTXHandle;
osThreadId_t TaskRGBHandle;
osThreadId_t TaskFlashLogHandle;

/* 任务属性 */
const osThreadAttr_t task_attr_normal = {
    .name = "Task", .priority = osPriorityNormal, .stack_size = 256
};
const osThreadAttr_t task_attr_high = {
    .name = "Task", .priority = osPriorityAboveNormal, .stack_size = 512
};
const osThreadAttr_t task_attr_low = {
    .name = "Task", .priority = osPriorityLow, .stack_size = 256
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

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ==================== printf 重定向 ==================== */

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
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

        if (ret == 0) {
            printf("[DHT11] T:%dC H:%d%%\r\n",
                   dht11_data.temperature, dht11_data.humidity);
            SystemStatus_FeedSensor();
            osEventFlagsSet(SystemEventFlags, EVENT_SENSOR_OK);
        } else {
            printf("[DHT11] Error: %d\r\n", ret);
            osEventFlagsSet(SystemEventFlags, EVENT_SENSOR_ERROR);
        }

        /* 非阻塞发送到队列 */
        sensor_data.dht11 = dht11_data;
        sensor_data.tick  = xTaskGetTickCount();
        osMessageQueuePut(SensorDataQueue, &sensor_data, 0, 0);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void Task_ADC(void *argument)
{
    ADC_Data_t adc_data;

    ADC_DMA_Init();

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
            if (HAL_CAN_AddTxMessage(&hcan, &tx_header,
                                     (uint8_t *)&data_frame,
                                     &tx_mailbox) == HAL_OK) {
                CAN_PrintSensorFrame(&data_frame);
            } else {
                printf("[CAN TX] Send Failed!\r\n");
            }

            /* 发送 0x202 状态帧 */
            SystemStatus_Check();
            CAN_PackStatusFrame(&status_frame,
                                SystemStatus_Get(),
                                SystemStatus_GetErrorCode());
            tx_header.StdId = CAN_ID_STATUS;
            tx_header.DLC   = 2;
            HAL_CAN_AddTxMessage(&hcan, &tx_header,
                                 (uint8_t *)&status_frame, &tx_mailbox);

            osEventFlagsSet(SystemEventFlags, EVENT_CAN_TX_DONE);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

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

void Task_FlashLog(void *argument)
{
    W25Q64_JEDEC_t jedec;
    FlashLogEntry_t entry;
    SensorData_t sensor_data;
    ADC_Data_t adc;

    W25Q64_Init();

    W25Q64_ReadJEDEC(&jedec);
    printf("[Flash] JEDEC ID: 0x%02X%02X%02X\r\n",
           jedec.manufacturer, jedec.memory_type, jedec.capacity);

    if (jedec.manufacturer != 0xEF ||
        jedec.memory_type  != 0x40 ||
        jedec.capacity     != 0x17) {
        printf("[Flash] WARNING: Unexpected JEDEC!\r\n");
    }

    W25Q64_LogInit();
    printf("[Flash] Log OK, Count: %lu\r\n", W25Q64_LogGetCount());

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        if (osMessageQueueGet(SensorDataQueue, &sensor_data, NULL, 0) == osOK) {
            ADC_DMA_Read(&adc);
            entry.timestamp   = xTaskGetTickCount() / 1000;
            entry.temperature = sensor_data.dht11.temperature;
            entry.humidity    = sensor_data.dht11.humidity;
            entry.adc_value   = adc.raw_value;
            entry.status      = sensor_data.dht11.status;

            W25Q64_LogAppend(&entry);
            printf("[Flash] Saved, Total: %lu\r\n",
                   W25Q64_LogGetCount());
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
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_CAN_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  /* 外设初始化 */
  SystemStatus_Init();
  HAL_CAN_Start(&hcan);

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
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */

  SensorDataQueue = osMessageQueueNew(SENSOR_QUEUE_SIZE,
                                      sizeof(SensorData_t), NULL);

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  TaskDHT11Handle    = osThreadNew(Task_DHT11,    NULL, &task_attr_normal);
  TaskADCHandle      = osThreadNew(Task_ADC,      NULL, &task_attr_normal);
  TaskCANTXHandle    = osThreadNew(Task_CAN_TX,   NULL, &task_attr_high);
  TaskRGBHandle      = osThreadNew(Task_RGB,      NULL, &task_attr_low);
  TaskFlashLogHandle = osThreadNew(Task_FlashLog, NULL, &task_attr_low);

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  SystemEventFlags = osEventFlagsNew(NULL);

  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  while (1)
  {
  }
}

/* ==================== 以下为 CubeMX 生成的硬件初始化函数, 无需修改 ==================== */

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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

/* ==================== 外设初始化函数 (CubeMX 自动生成) ==================== */

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_CAN_Init(void)
{
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
}

static void MX_SPI2_Init(void)
{
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
}

static void MX_TIM3_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
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
}

static void MX_USART1_UART_Init(void)
{
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
}

static void MX_GPIO_Init(void)
{
  /* CubeMX 自动生成, 此处留空 */
}

static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

void StartDefaultTask(void *argument)
{
  for (;;)
  {
    osDelay(1);
  }
}

/* ==================== HAL 回调 ==================== */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  /* ADC DMA 循环模式, 每转换完成一次触发, 无需额外处理 */
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}
