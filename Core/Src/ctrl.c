#include "ctrl.h"
#include "main.h"
#include "oled.h"
#include "stdio.h"
#include "string.h"
#include "ws2812.h"
#include "kk_rtc.h"
#include "rs232.h"
#include "music.h"

#define VERSION "KK REMINDER V0.9"
extern TIM_HandleTypeDef htim1;  // 編碼器使用的定時器

static void update_oled_datetime(void);

/* 狀態定義 */
typedef enum
{
  STATE_IDLE = 0,       // 待機中，等待編碼器調整
  STATE_RTC_SETUP,      // RTC 年月日時分秒調整
  STATE_COUNTING,       // 倒計時中
  STATE_ALARM           // 提醒中
} SystemState_t;

typedef enum
{
  RTC_EDIT_YEAR = 0,
  RTC_EDIT_MONTH,
  RTC_EDIT_DAY,
  RTC_EDIT_HOUR,
  RTC_EDIT_MINUTE,
  RTC_EDIT_SECOND,
  RTC_EDIT_CONFIRM
} RtcEditStep_t;

typedef enum
{
  RTC_CONFIRM_CANCEL = 0,
  RTC_CONFIRM_SAVE
} RtcConfirmChoice_t;

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
static int16_t encoder_step_accumulator = 0;  // TI2 模式下每格約 2 個計數，累積後再換算成一步
static struct tm rtc_edit_time;
static RtcEditStep_t rtc_edit_step = RTC_EDIT_YEAR;
static RtcConfirmChoice_t rtc_confirm_choice = RTC_CONFIRM_SAVE;

/* LED 輪流點亮變數 */
typedef enum
{
  LED_STATE_RED = 0,
  LED_STATE_GREEN = 1,
  LED_STATE_BLUE = 2
} LED_State_t;
static LED_State_t current_led_state = LED_STATE_RED;
static uint32_t last_led_tick = 0;      // LED 狀態更新時間戳

static const char *rtc_weekday_text(int wday)
{
  static const char *const weekday_names[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
  };

  if (wday < 0 || wday > 6)
  {
    return "???";
  }

  return weekday_names[wday];
}

static int rtc_days_in_month(int year, int month)
{
  static const int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  int days = days_in_month[month - 1];

  if (month == 2)
  {
    if (((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0))
    {
      days = 29;
    }
  }

  return days;
}

static void rtc_edit_sync_weekday(void)
{
  time_t unix_time = mktime(&rtc_edit_time);
  struct tm *normalized = gmtime(&unix_time);

  if (normalized != NULL)
  {
    rtc_edit_time = *normalized;
  }
}

static void rtc_edit_load_current_time(void)
{
  struct tm *now = KK_RTC_GetTime();

  if (now != NULL)
  {
    rtc_edit_time = *now;
  }
  else
  {
    memset(&rtc_edit_time, 0, sizeof(rtc_edit_time));
    rtc_edit_time.tm_year = 2025 - 1900;
    rtc_edit_time.tm_mon = 0;
    rtc_edit_time.tm_mday = 1;
  }

  rtc_edit_sync_weekday();
}

static void rtc_edit_begin(void)
{
  rtc_edit_load_current_time();
  rtc_edit_step = RTC_EDIT_YEAR;
  rtc_confirm_choice = RTC_CONFIRM_SAVE;
  __HAL_TIM_SET_COUNTER(&htim1, 0);
  last_encoder_count = 0;
}

static void rtc_edit_adjust_value(int delta)
{
  while (delta != 0)
  {
    int direction = (delta > 0) ? 1 : -1;

    switch (rtc_edit_step)
    {
      case RTC_EDIT_YEAR:
      {
        int year = rtc_edit_time.tm_year + 1900;
        year += direction;
        if (year > 2099) year = 2000;
        if (year < 2000) year = 2099;
        rtc_edit_time.tm_year = year - 1900;
        break;
      }
      case RTC_EDIT_MONTH:
      {
        int month = rtc_edit_time.tm_mon + 1;
        month += direction;
        if (month > 12) month = 1;
        if (month < 1) month = 12;
        rtc_edit_time.tm_mon = month - 1;

        int days = rtc_days_in_month(rtc_edit_time.tm_year + 1900, month);
        if (rtc_edit_time.tm_mday > days)
        {
          rtc_edit_time.tm_mday = days;
        }
        break;
      }
      case RTC_EDIT_DAY:
      {
        int year = rtc_edit_time.tm_year + 1900;
        int month = rtc_edit_time.tm_mon + 1;
        int days = rtc_days_in_month(year, month);
        int day = rtc_edit_time.tm_mday + direction;

        if (day > days) day = 1;
        if (day < 1) day = days;
        rtc_edit_time.tm_mday = day;
        break;
      }
      case RTC_EDIT_HOUR:
      {
        int hour = rtc_edit_time.tm_hour + direction;
        if (hour > 23) hour = 0;
        if (hour < 0) hour = 23;
        rtc_edit_time.tm_hour = hour;
        break;
      }
      case RTC_EDIT_MINUTE:
      {
        int minute = rtc_edit_time.tm_min + direction;
        if (minute > 59) minute = 0;
        if (minute < 0) minute = 59;
        rtc_edit_time.tm_min = minute;
        break;
      }
      case RTC_EDIT_SECOND:
      {
        int second = rtc_edit_time.tm_sec + direction;
        if (second > 59) second = 0;
        if (second < 0) second = 59;
        rtc_edit_time.tm_sec = second;
        break;
      }
      case RTC_EDIT_CONFIRM:
        rtc_confirm_choice = (rtc_confirm_choice == RTC_CONFIRM_SAVE) ? RTC_CONFIRM_CANCEL : RTC_CONFIRM_SAVE;
        break;
      default:
        break;
    }

    rtc_edit_sync_weekday();
    delta -= direction;
  }
}

static void rtc_edit_next_step(void)
{
  if (rtc_edit_step < RTC_EDIT_CONFIRM)
  {
    rtc_edit_step = (RtcEditStep_t)(rtc_edit_step + 1);
    return;
  }

  if (rtc_confirm_choice == RTC_CONFIRM_SAVE)
  {
    (void)KK_RTC_SetTime(&rtc_edit_time);
  }

  current_state = STATE_IDLE;
  __HAL_TIM_SET_COUNTER(&htim1, 0);
  last_encoder_count = 0;
}

static void update_oled_rtc_setup(void)
{
  OLED_NewFrame();

  char buf[32];
  sprintf(buf, "%04d/%02d/%02d", rtc_edit_time.tm_year + 1900, rtc_edit_time.tm_mon + 1, rtc_edit_time.tm_mday);
  OLED_PrintString(0, 0, buf, &font16x16, OLED_COLOR_NORMAL);

  sprintf(buf, "%02d:%02d:%02d", rtc_edit_time.tm_hour, rtc_edit_time.tm_min, rtc_edit_time.tm_sec);
  OLED_PrintString(0, 16, buf, &font16x16, OLED_COLOR_NORMAL);

  sprintf(buf, "WEEK: %s", rtc_weekday_text(rtc_edit_time.tm_wday));
  OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);

  if (rtc_edit_step == RTC_EDIT_CONFIRM)
  {
    if (rtc_confirm_choice == RTC_CONFIRM_SAVE)
    {
      OLED_PrintString(0, 48, "CANCE  >SAVE", &font16x16, OLED_COLOR_NORMAL);
    }
    else
    {
      OLED_PrintString(0, 48, ">CANCE  SAVE", &font16x16, OLED_COLOR_NORMAL);
    }
  }
  else
  {
    switch (rtc_edit_step)
    {
      case RTC_EDIT_YEAR:
        OLED_PrintString(0, 48, "YEAR  KEY:NEXT", &font16x16, OLED_COLOR_NORMAL);
        break;
      case RTC_EDIT_MONTH:
        OLED_PrintString(0, 48, "MONTH KEY:NEXT", &font16x16, OLED_COLOR_NORMAL);
        break;
      case RTC_EDIT_DAY:
        OLED_PrintString(0, 48, "DAY   KEY:NEXT", &font16x16, OLED_COLOR_NORMAL);
        break;
      case RTC_EDIT_HOUR:
        OLED_PrintString(0, 48, "HOUR  KEY:NEXT", &font16x16, OLED_COLOR_NORMAL);
        break;
      case RTC_EDIT_MINUTE:
        OLED_PrintString(0, 48, "MIN   KEY:NEXT", &font16x16, OLED_COLOR_NORMAL);
        break;
      case RTC_EDIT_SECOND:
        OLED_PrintString(0, 48, "SEC   KEY:NEXT", &font16x16, OLED_COLOR_NORMAL);
        break;
      default:
        break;
    }
  }

  OLED_ShowFrame();
}



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
  OLED_PrintString(0, 0, VERSION, &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 16, "STATUS: IDLE    ", &font16x16, OLED_COLOR_NORMAL);
  char buf[32];
  // sprintf(buf, "TIMER:  %02d mins", set_time_minutes);
  //OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  sprintf(buf, "TIMER: %02d:%02dmins", set_time_minutes, 0);
  OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  update_oled_datetime();
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
  OLED_PrintString(0, 0, VERSION, &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 16, "STATUS: COUNTING", &font16x16, OLED_COLOR_NORMAL);
  /* 顯示倒計時 */
  uint16_t minutes = remaining_seconds / 60;
  uint16_t seconds = remaining_seconds % 60;
  char buf[32];
  // sprintf(buf, "TIMER:  %02d mins", set_time_minutes);
  // OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  sprintf(buf, "TIMER: %02d:%02dmins", minutes, seconds);
  OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  /* 顯示進度 */
  uint16_t progress = 0;
  if (total_seconds > 0)
  {
    progress = ((total_seconds - remaining_seconds) * 100) / total_seconds;
  }
  sprintf(buf, "Progress: %d%%", progress);
  // OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  // OLED_PrintString(0, 48, "Press: Reset", &font16x16, OLED_COLOR_NORMAL);
  update_oled_datetime();
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
    OLED_PrintString(20, 0, "TIME IS UP!", &font16x16, OLED_COLOR_NORMAL);
    OLED_PrintString(15, 16, "DRINK WATER!", &font16x16, OLED_COLOR_NORMAL);
  }
  
  OLED_PrintString(0, 32, "Press: Reset", &font16x16, OLED_COLOR_NORMAL);
  update_oled_datetime();
  
  OLED_ShowFrame();
}

static void update_oled_datetime(void)
{
  struct tm *now = KK_RTC_GetTime();
  if (now == NULL) {
    return;
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min);
  OLED_PrintString(0, 48, buf, &font16x16, OLED_COLOR_NORMAL);
}

/* 編碼器處理 */
void encoder_task(void)
{
  uint16_t current_count = __HAL_TIM_GET_COUNTER(&htim1);
  
  /* 檢測編碼器變化 */
  int16_t delta = (int16_t)(current_count - last_encoder_count);
  
  if (delta != 0)
  {
    encoder_step_accumulator += delta;

    while (encoder_step_accumulator >= 2)
    {
      if (current_state == STATE_IDLE)
      {
        if (set_time_minutes > 10)
        {
          set_time_minutes--;
        }
      }
      else if (current_state == STATE_RTC_SETUP)
      {
        rtc_edit_adjust_value(-1);
      }

      encoder_step_accumulator -= 2;
    }

    while (encoder_step_accumulator <= -2)
    {
      if (current_state == STATE_IDLE)
      {
        if (set_time_minutes < 60)
        {
          set_time_minutes++;
        }
      }
      else if (current_state == STATE_RTC_SETUP)
      {
        rtc_edit_adjust_value(1);
      }

      encoder_step_accumulator += 2;
    }

    last_encoder_count = current_count;
  }
}

/* 按鈕處理 */
void button_task(void)
{
  if (current_state == STATE_RTC_SETUP)
  {
    if(key_state == KeyNotPressed)
    {
      if(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)
      {
        HAL_Delay(20);  // 防抖延時
        if(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)
        {
          key_state = KeyWasPressed;
          rtc_edit_next_step();
          HAL_Delay(20);  // 防抖延時
        }
      }
    }
    else
    {
      if(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
      {
        HAL_Delay(20);  // 防抖延時
        if(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
        {
          key_state = KeyNotPressed;
        }
      }
    }

    if(key2_state == KeyNotPressed)
    {
      if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
      {
        HAL_Delay(20);  // 防抖延時
        if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
        {
          key2_state = KeyWasPressed;
        }
      }
    }
    else
    {
      if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_SET)
      {
        HAL_Delay(20);  // 防抖延時
        if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_SET)
        {
          key2_state = KeyNotPressed;
        }
      }
    }

    return;
  }

  if(key_state == KeyNotPressed)
  {
    if(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)
    {
      HAL_Delay(20);  // 防抖延時
      if(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)
      {
        key_state = KeyWasPressed;
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
          stop_music();
          HAL_Delay(20);  // 防抖延時
        }  
      }
    }
  }
  else //防止按鍵重入，必須等按鍵釋放後才能再次觸發
  { 
    if(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
    {
      HAL_Delay(20);  // 防抖延時
      if(HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
      {
        key_state = KeyNotPressed;
      }
    }
  }

  if(key2_state == KeyNotPressed)
  {
    if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
    {
      HAL_Delay(20);  // 防抖延時
      if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
      {
        if (current_state == STATE_IDLE)
        {
          current_state = STATE_RTC_SETUP;
          rtc_edit_begin();
        }
        key2_state = KeyWasPressed;
      }
    }
  }
  else
  {
    if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_SET)
    {
      HAL_Delay(20);  // 防抖延時
      if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_SET)
      {
        key2_state = KeyNotPressed;
      }
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
      start_music();
    }
  }
}

/* 提醒處理 */
void alarm_task(void)
{
  /* 在 STATE_ALARM 時播放音樂與 WS2812 彩虹效果 */
  if (current_state == STATE_ALARM)
  {
    play_music();
    ws2812_rainbow(50);
  }
  else
  {
    /* 其他狀態時關閉 WS2812，避免殘留色彩 */
    ws2812_set_all(0);
    ws2812_update();
  }
}

/* LED 輪流點亮任務 - 只在倒計時狀態時每秒鐘輪流點亮紅、綠、藍 */
void led_rotate_task(void)
{
  uint32_t now = HAL_GetTick();
  
  /* 只在 STATE_COUNTING 時執行 LED 輪流點亮 */
  if (current_state == STATE_COUNTING)
  {
    /* 每秒鐘切換一次 LED */
    if (now - last_led_tick >= 1000)
    {
      last_led_tick = now;
      
      /* 關閉所有 LED */
      HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
      
      /* 根據當前狀態點亮對應的 LED */
      switch (current_led_state)
      {
        case LED_STATE_RED:
          HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
          current_led_state = LED_STATE_GREEN;
          break;
        case LED_STATE_GREEN:
          HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
          current_led_state = LED_STATE_BLUE;
          break;
        case LED_STATE_BLUE:
          HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
          current_led_state = LED_STATE_RED;
          break;
        default:
          break;
      }
    }
  }
  else
  {
    /* 非 STATE_COUNTING 狀態時，關閉所有 LED */
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
  }
}

void reset_task(void)
{
  if(key1_state == KeyNotPressed)
  {
    if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
    {
      HAL_Delay(20);  // 防抖延時
      if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
      {
        HAL_NVIC_SystemReset();  // 觸發系統重置
      }
    }
  }
  else //防止按鍵重入，必須等按鍵釋放後才能再次觸發
  { 
    if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET)
    {
      HAL_Delay(20);  // 防抖延時
      if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET)
      {
        key1_state = KeyNotPressed;
      }
    }
  }
}

void setup(void)
{
 /* 初始化蜂鳴器（PWM） */
  HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 0);
  init_music();
  
  /* 初始化編碼器 */
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_2);
  __HAL_TIM_SET_COUNTER(&htim1, 0);
  last_encoder_count = 0;
  
  /* 初始化 LED */
  current_led_state = LED_STATE_RED;
  last_led_tick = HAL_GetTick();
  
  /* 初始化 OLED */
  HAL_Delay(60);  // 等待 OLED 穩定
  OLED_Init();
  OLED_NewFrame();
  OLED_PrintString(0, 0, VERSION, &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 16, "STATUS: IDLE    ", &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 32, "TIMER: 15:00mins", &font16x16, OLED_COLOR_NORMAL);
  // OLED_PrintString(0, 48, "COUNTDOWN: 15:00", &font16x16, OLED_COLOR_NORMAL);
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
  reset_task();        // 處理重置邏輯
  led_rotate_task();   // LED 輪流點亮任務
  
  /* 根據狀態更新 OLED 顯示 */
  switch (current_state)
  {
    case STATE_IDLE:
      update_oled_idle();
      break;
    case STATE_RTC_SETUP:
      update_oled_rtc_setup();
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