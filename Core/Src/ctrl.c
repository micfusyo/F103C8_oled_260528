#include "ctrl.h"
#include "oled.h"
#include "stdio.h"
#include "string.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;  // 編碼器使用的定時器
extern TIM_HandleTypeDef htim4;  // 蜂鳴器使用的定時器

/* 狀態定義 */
typedef enum
{
  STATE_IDLE = 0,       // 待機中，等待編碼器調整
  STATE_COUNTING,       // 倒計時中
  STATE_ALARM           // 提醒中
} SystemState_t;

typedef enum
{
  KeyNotPressed = 0,
  KeyWasPressed
} KeyState_t;

/* 系統變數 */
SystemState_t current_state = STATE_IDLE;
KeyState_t key_state = KeyWasPressed;
KeyState_t key1_state = KeyWasPressed;
KeyState_t key2_state = KeyWasPressed;
uint16_t set_time_minutes = 15;         // 設置的提醒時間（分鐘）：10-60
uint16_t remaining_seconds = 0;         // 剩餘秒數
uint16_t total_seconds = 0;             // 總秒數（用於計算進度條）

/* 編碼器變數 */
uint16_t last_encoder_count = 0;        // 上次編碼器計數
static uint32_t last_second_tick = 0;   // 上次秒級更新時間
static uint32_t last_button_tick = 0;   // 按鈕防抖時間
static uint8_t alarm_blink_flag = 0;    // 提醒閃爍標誌


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

void setup(void)
{
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
}

void loop(void)
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
}