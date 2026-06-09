/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Board2 — 显示监控节点  FreeRTOS + CAN RX + OLED
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "rgb_led.h"
#include "oled.h"
#include "can_protocol.h"
#include "can_receiver.h"
#include "system_status.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EVENT_DATA_UPDATED    0x0001
#define EVENT_STATUS_UPDATED  0x0002
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* FreeRTOS 对象 */
osSemaphoreId_t  CANSemaphore;
osEventFlagsId_t DisplayEventFlags;

/* 任务句柄 */
osThreadId_t TaskCANRXHandle;
osThreadId_t TaskOLEDHandle;
osThreadId_t TaskRGBHandle;

/* 任务属性 */
const osThreadAttr_t task_attr_normal = {
    .name = "Task", .priority = osPriorityNormal, .stack_size = 512
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
static void MX_CAN_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

void Task_CAN_RX(void *argument);
void Task_OLED(void *argument);
void Task_RGB(void *argument);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ==================== printf 重定向 ==================== */

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/* ==================== CAN RX 中断回调 ==================== */

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0,
                             &rx_header, rx_data) == HAL_OK) {
        CAN_Receiver_ProcessRx(&rx_header, rx_data);
    }
    osSemaphoreRelease(CANSemaphore);
}

/* ==================== 任务实现 ==================== */

void Task_CAN_RX(void *argument)
{
    CAN_Receiver_Init();
    CAN_Receiver_Start();

    printf("[CAN RX] Ready, Filter: 0x201 & 0x202\r\n");

    for (;;) {
        if (osSemaphoreAcquire(CANSemaphore, pdMS_TO_TICKS(2000)) == osOK) {
            SystemStatus_FeedCAN();
            osEventFlagsSet(DisplayEventFlags, EVENT_DATA_UPDATED);
        } else {
            printf("[CAN RX] Timeout!\r\n");
            osEventFlagsSet(DisplayEventFlags, EVENT_STATUS_UPDATED);
        }
    }
}

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
        SystemStatus_Check();
        SystemStatus_t status = SystemStatus_Get();

        if (g_can_rx_data.sensor_updated) {
            CAN_SensorFrame_t *s = &g_can_rx_data.sensor;

            snprintf(line1, sizeof(line1), "Temp: %dC", s->temp_int);
            snprintf(line2, sizeof(line2), "Humi: %d%%", s->humi_int);
            snprintf(line3, sizeof(line3), "ADC: %d (%.2fV)",
                     s->adc_value,
                     (float)s->adc_value / 4095.0f * 3.3f);
            g_can_rx_data.sensor_updated = 0;
        } else {
            snprintf(line1, sizeof(line1), "Waiting data...");
            line2[0] = '\0';
            line3[0] = '\0';
        }

        snprintf(line4, sizeof(line4), "[%s]", SystemStatus_GetString());

        /* 刷新 OLED */
        OLED_ShowString(0, 0, line1);
        OLED_ShowString(0, 2, line2);
        OLED_ShowString(0, 4, line3);

        /* 状态栏高亮 */
        if (status != prev_status) {
            OLED_ShowHighlight(7);
            prev_status = status;
        }
        OLED_ShowString(0, 7, line4);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void Task_RGB(void *argument)
{
    RGB_Init();
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) {
        SystemStatus_Check();

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
  MX_CAN_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  SystemStatus_Init();

  printf("\r\n========================================\r\n");
  printf("  Board2: Display Node\r\n");
  printf("  STM32F103C8T6  FreeRTOS  CAN 500kbps\r\n");
  printf("========================================\r\n");

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */

  CANSemaphore = osSemaphoreNew(10, 0, NULL);

  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  TaskCANRXHandle = osThreadNew(Task_CAN_RX, NULL, &task_attr_high);
  TaskOLEDHandle  = osThreadNew(Task_OLED,  NULL, &task_attr_normal);
  TaskRGBHandle   = osThreadNew(Task_RGB,   NULL, &task_attr_low);

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  DisplayEventFlags = osEventFlagsNew(NULL);

  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  while (1)
  {
  }
}

/* ==================== 硬件初始化 (CubeMX 自动生成) ==================== */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
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
}

void StartDefaultTask(void *argument)
{
  for (;;)
  {
    osDelay(1);
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}
