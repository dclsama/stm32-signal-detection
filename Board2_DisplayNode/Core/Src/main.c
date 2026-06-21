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

/* Definitions for defaultTask */
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

/* 任务属性 */
const osThreadAttr_t task_attr_normal = {
    .name = "Task", .priority = osPriorityNormal, .stack_size = 1536
};
const osThreadAttr_t task_attr_high = {
    .name = "Task", .priority = osPriorityAboveNormal, .stack_size = 1536
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
    CAN_TxHeaderTypeDef cmd_tx = {0};
    uint8_t cmd_data = 0xAA;
    uint32_t tx_mailbox;
    uint8_t alarm_sent = 0;

    CAN_Receiver_Init();
    CAN_Receiver_Start();

    cmd_tx.StdId = CAN_ID_ACK;    /* 0x301 */
    cmd_tx.ExtId = 0;
    cmd_tx.IDE   = CAN_ID_STD;
    cmd_tx.RTR   = CAN_RTR_DATA;
    cmd_tx.DLC   = 1;

    printf("[CAN RX] Ready, Filter: 0x201 & 0x202\r\n");

    for (;;) {
        uint8_t got_data = 0;
        if (osSemaphoreAcquire(CANSemaphore, pdMS_TO_TICKS(5000)) == osOK) {
            CAN_Receiver_ProcessTask();
            SystemStatus_FeedCAN();   /* 收到数据才喂 → 断线可检测 */
            SystemStatus_FeedSensor();
            got_data = 1;
        }
        /* 超时不喂狗 → 断线自动触发 ERROR:CAN */

        /* ==== 同步 Board1 状态到本地 ==== */
        if (got_data && g_can_rx_data.status_updated) {
            uint8_t st = g_can_rx_data.status.system_status;
            SystemStatus_Set((SystemStatus_t)st,
                             g_can_rx_data.status.error_code);
            g_can_rx_data.status_updated = 0;

            /* ==== 命令帧: 检测到告警时发 0x301 清告警 ==== */
            if ((st == SYSTEM_ERROR1 || st == SYSTEM_ERROR2) && !alarm_sent) {
                /* 发送 0x301 命令 → Board1 清告警 */
                if (HAL_CAN_AddTxMessage(&hcan, &cmd_tx,
                                         &cmd_data, &tx_mailbox) == HAL_OK) {
                    printf("[CAN TX] Command 0x301 → Clear Alarm\r\n");
                    alarm_sent = 1;
                }
            }
            if (st == SYSTEM_SAFE) {
                alarm_sent = 0;  /* 恢复后可再次发命令 */
            }
        }
    }
}

void Task_OLED(void *argument)
{
    char line0[22], line1[22], line2[22], line3[22], line4[22], lineT[22];
    static CAN_SensorFrame_t last_sensor = {0};
    static uint8_t has_data = 0;
    SystemStatus_t prev_status = SYSTEM_SAFE;

    OLED_Init();
    OLED_Clear();

    /* 启动画面 */
    OLED_ShowString(18, 1, "Signal Detection");
    OLED_ShowString(22, 3, "Board 2: RX");
    vTaskDelay(pdMS_TO_TICKS(1500));
    OLED_Clear();

    for (;;) {
        SystemStatus_FeedSensor();
        SystemStatus_Check();
        SystemStatus_t status = SystemStatus_Get();

        /* 更新最新传感器数据 */
        if (g_can_rx_data.sensor_updated) {
            memcpy(&last_sensor, &g_can_rx_data.sensor, sizeof(last_sensor));
            g_can_rx_data.sensor_updated = 0;
            has_data = 1;
        }

        if (has_data) {
            /* 避免 %%f (newlib-nano 不支持) — 用整数运算 */
            unsigned int v1_int = (unsigned int)(last_sensor.adc1 * 330U / 255);
            unsigned int v2_int = (unsigned int)(last_sensor.adc2 * 330U / 255);
            snprintf(line0, sizeof(line0), "Thr:V<%u.%02uV",
                     last_sensor.volt_threshold / 100,
                     last_sensor.volt_threshold % 100);
            snprintf(lineT, sizeof(lineT), "Thr:T>%dC",
                     (int)g_can_rx_data.status.temp_threshold);
            snprintf(line1, sizeof(line1), "T:%dC  H:%d%%",
                     last_sensor.temp_int, last_sensor.humi_int);
            snprintf(line2, sizeof(line2), "V1:%u.%02uV V2:%u.%02uV",
                     v1_int / 100, v1_int % 100,
                     v2_int / 100, v2_int % 100);
            snprintf(line3, sizeof(line3), "RX:%lu st=%u/%s",
                     (unsigned long)g_can_rx_data.rx_count,
                     g_can_rx_data.status.system_status,
                     SystemStatus_GetString());
        } else {
            snprintf(line0, sizeof(line0), "Thr:V<--.--V");
            snprintf(lineT, sizeof(lineT), "Thr:T>--C");
            snprintf(line1, sizeof(line1), "Waiting CAN...");
            snprintf(line2, sizeof(line2), "RX frames: 0");
            snprintf(line3, sizeof(line3), "");
        }

        /* 第四行: 系统状态 (已由 Task_CAN_RX 同步 Board1 状态) */
        snprintf(line4, sizeof(line4), "[%s]", SystemStatus_GetString());

        /* 填充空格到 21 字符, 彻底清除旧内容 (line1/2/3/4) */
        for (uint8_t li = 0; li < 4; li++) {
            char *lp = (li == 0) ? line1 : (li == 1) ? line2 :
                       (li == 2) ? line3 : line4;
            int l = strlen(lp);
            while (l < 21) lp[l++] = ' ';
            lp[21] = '\0';
        }
        /* line0/lineT 也填充到 21 字符 */
        for (uint8_t li = 0; li < 2; li++) {
            char *lp = (li == 0) ? line0 : lineT;
            int l = strlen(lp);
            while (l < 21) lp[l++] = ' ';
            lp[21] = '\0';
        }

        /* 刷新 OLED: Page 0=V阈值, Page 1=温湿度, Page 2=T阈值, Page 3=电压, Page 5=状态, Page 7=系统 */
        OLED_ShowString(0, 0, line0);
        OLED_ShowString(0, 1, line1);
        OLED_ShowString(0, 2, lineT);
        OLED_ShowString(0, 3, line2);
        OLED_ShowString(0, 5, line3);

        /* 状态栏: 告警时反色高亮 */
        if (status != prev_status) {
            if (status != SYSTEM_SAFE) {
                OLED_ShowHighlight(7);
            }
            prev_status = status;
        }
        OLED_ShowString(0, 7, line4);

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
  MX_CAN_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  SystemStatus_Init();

  /* 禁用 printf 缓冲 */
  setvbuf(stdout, NULL, _IONBF, 0);

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
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  TaskCANRXHandle = osThreadNew(Task_CAN_RX, NULL, &task_attr_high);
  TaskOLEDHandle  = osThreadNew(Task_OLED,  NULL, &task_attr_normal);

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  DisplayEventFlags = osEventFlagsNew(NULL);

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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
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
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

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
