/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "stdio.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* 狀態定義 */
typedef enum
{
  STATE_IDLE = 0,       // 待機中，等待編碼器調整
  STATE_COUNTING,       // 倒計時中
  STATE_ALARM           // 提醒中
} SystemState_t;

/* 系統變數 */
SystemState_t current_state = STATE_IDLE;
uint16_t set_time_minutes = 15;         // 設置的提醒時間（分鐘）：10-60
uint16_t remaining_seconds = 0;         // 剩餘秒數
uint16_t total_seconds = 0;             // 總秒數（用於計算進度條）

/* 編碼器變數 */
uint16_t last_encoder_count = 0;        // 上次編碼器計數
static uint32_t last_second_tick = 0;   // 上次秒級更新時間
static uint32_t last_button_tick = 0;   // 按鈕防抖時間
static uint8_t alarm_blink_flag = 0;    // 提醒閃爍標誌

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void update_oled_idle(void);
void update_oled_counting(void);
void update_oled_alarm(void);
void encoder_task(void);
void button_task(void);
void timer_task(void);
void alarm_task(void);
void draw_progress_bar(uint16_t x, uint16_t y, uint16_t width, uint16_t current, uint16_t total);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* 繪製進度條 */
void draw_progress_bar(uint16_t x, uint16_t y, uint16_t width, uint16_t current, uint16_t total)
{
  if (total == 0) return;
  uint16_t filled = (current * width) / total;
  (void)filled;  // 保留函數接口，但不使用（可後續擴展）
}

/* OLED 顯示函數 - 待機狀態 */
void update_oled_idle(void)
{
  static uint32_t last_blink = 0;
  uint32_t now = HAL_GetTick();
  if (now - last_blink < 100) return;  // 100ms 更新一次
  last_blink = now;
  OLED_NewFrame();
  OLED_PrintString(0, 0, " WATER REMINDER ", &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 16, "STATUS: IDLE    ", &font16x16, OLED_COLOR_NORMAL);
  char buf[32];
  sprintf(buf, "TIMER:  %02d mins", set_time_minutes);
  OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  sprintf(buf, "COUNTDOWN: %02d:%02d", set_time_minutes, 0);
  OLED_PrintString(0, 48, buf, &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();
}

/* OLED 顯示函數 - 倒計時狀態 */
void update_oled_counting(void)
{
  static uint32_t last_update = 0;
  uint32_t now = HAL_GetTick();
  if (now - last_update < 500) return;  // 500ms 更新一次
  last_update = now;
  OLED_NewFrame();
  OLED_PrintString(0, 0, " WATER REMINDER ", &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 16, "STATUS: COUNTING", &font16x16, OLED_COLOR_NORMAL);
  /* 顯示倒計時 */
  uint16_t minutes = remaining_seconds / 60;
  uint16_t seconds = remaining_seconds % 60;
  char buf[32];
  sprintf(buf, "TIMER:  %02d mins", set_time_minutes);
  OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  sprintf(buf, "COUNTDOWN: %02d:%02d", minutes, seconds);
  OLED_PrintString(0, 48, buf, &font16x16, OLED_COLOR_NORMAL);
  /* 顯示進度 */
  uint16_t progress = 0;
  if (total_seconds > 0)
  {
    progress = ((total_seconds - remaining_seconds) * 100) / total_seconds;
  }
  sprintf(buf, "Progress: %d%%", progress);
  // OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  // OLED_PrintString(0, 48, "Press: Reset", &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();
}

/* OLED 顯示函數 - 提醒狀態 */
void update_oled_alarm(void)
{
  static uint32_t last_blink = 0;
  uint32_t now = HAL_GetTick();
  
  if (now - last_blink < 300) return;  // 300ms 閃爍
  last_blink = now;
  
  alarm_blink_flag = !alarm_blink_flag;
  
  OLED_NewFrame();
  
  if (alarm_blink_flag)
  {
    OLED_PrintString(20, 16, "TIME IS UP!", &font16x16, OLED_COLOR_NORMAL);
    OLED_PrintString(15, 32, "DRINK WATER!", &font16x16, OLED_COLOR_NORMAL);
  }
  
  OLED_PrintString(0, 48, "Press: Reset", &font16x16, OLED_COLOR_NORMAL);
  
  OLED_ShowFrame();
}

/* 編碼器處理 */
void encoder_task(void)
{
  if (current_state != STATE_IDLE) return;  // 只在待機狀態調整時間
  
  uint16_t current_count = __HAL_TIM_GET_COUNTER(&htim1);
  
  /* 檢測編碼器變化 */
  int16_t delta = (int16_t)(current_count - last_encoder_count);
  
  if (delta != 0)
  {
    if (delta > 0)
    {
      /* 順時針旋轉，增加時間 */
      if (set_time_minutes < 60)
      {
        set_time_minutes++;
      }
    }
    else
    {
      /* 逆時針旋轉，減少時間 */
      if (set_time_minutes > 10)
      {
        set_time_minutes--;
      }
    }
    
    last_encoder_count = current_count;
  }
}

/* 按鈕處理 */
void button_task(void)
{
  uint32_t now = HAL_GetTick();
  
  /* 防抖 */
  if (now - last_button_tick < 20) return;
  
  if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)
  {
    last_button_tick = now;
    
    if (current_state == STATE_IDLE)
    {
      /* 待機狀態 -> 倒計時狀態 */
      current_state = STATE_COUNTING;
      total_seconds = set_time_minutes * 60;
      remaining_seconds = total_seconds;
      HAL_Delay(20);  // 防抖延時
    }
    else if (current_state == STATE_COUNTING || current_state == STATE_ALARM)
    {
      /* 倒計時/提醒 -> 待機狀態 */
      current_state = STATE_IDLE;
      remaining_seconds = 0;
      total_seconds = 0;
      HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
      HAL_Delay(20);  // 防抖延時
    }
  }
}

/* 倒計時處理 */
void timer_task(void)
{
  uint32_t now = HAL_GetTick();
  
  if (current_state != STATE_COUNTING) return;
  
  /* 每秒檢測一次 */
  if (now - last_second_tick >= 1000)
  {
    last_second_tick = now;
    
    if (remaining_seconds > 0)
    {
      remaining_seconds--;
    }
    else
    {
      /* 時間到，進入提醒狀態 */
      current_state = STATE_ALARM;
      HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
    }
  }
}

/* 提醒處理 */
void alarm_task(void)
{
  /* 在STATE_ALARM時，蜂鳴器已經啟動，等待按鈕重置 */
  /* 無需額外處理 */
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
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 初始化蜂鳴器（PWM） */
  HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 500);  // 設置占空比 50%
  
  /* 初始化編碼器 */
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_2);
  __HAL_TIM_SET_COUNTER(&htim1, 0);
  last_encoder_count = 0;
  
  /* 初始化 OLED */
  HAL_Delay(60);  // 等待 OLED 穩定
  OLED_Init();
  OLED_NewFrame();
  OLED_PrintString(0, 0, " WATER REMINDER ", &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 16, "STATUS: IDLE    ", &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 32, "TIMER:  15 mins ", &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 48, "COUNTDOWN: 15:00", &font16x16, OLED_COLOR_NORMAL);
  OLED_ShowFrame();
  HAL_Delay(1000);
  
  /* 進入待機狀態 */
  current_state = STATE_IDLE;
  set_time_minutes = 15;  // 默認 15 分鐘
  last_second_tick = HAL_GetTick();
  last_button_tick = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 任務調度 */
    encoder_task();      // 處理編碼器輸入
    button_task();       // 處理按鈕輸入
    timer_task();        // 處理倒計時邏輯
    alarm_task();        // 處理提醒邏輯
    
    /* 根據狀態更新 OLED 顯示 */
    switch (current_state)
    {
      case STATE_IDLE:
        update_oled_idle();
        break;
      case STATE_COUNTING:
        update_oled_counting();
        break;
      case STATE_ALARM:
        update_oled_alarm();
        break;
      default:
        break;
    }
    
    HAL_Delay(1);  // 短延時，降低 CPU 佔用
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

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
