#include "ctrl.h"
#include "oled.h"
#include "stdio.h"
#include "stm32f1xx_hal_cortex.h"
#include "string.h"
#include "main.h"
#include "ws2812.h"

extern TIM_HandleTypeDef htim1;  // 編碼器使用的定時器
extern TIM_HandleTypeDef htim4;  // 蜂鳴器使用的定時器

void init_music(void);
void start_music(void);
void stop_music(void);
void play_music(void);

#define P0 	0	// 休止符频率

#define L1 262  // 低音频率
#define L2 294
#define L3 330
#define L4 349
#define L5 392
#define L6 440
#define L7 494

#define M1 523  // 中音频率
#define M2 587
#define M3 659
#define M4 698
#define M5 784
#define M6 880
#define M7 988

#define H1 1047 // 高音频率
#define H2 1175
#define H3 1319
#define H4 1397
#define H5 1568
#define H6 1760
#define H7 1976

typedef struct
{
    uint16_t frequency;
    float period;
} Bate;

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

const Bate MoChouXiang[] = {
    // 我被困在了
    {M6, 1}, {M5, 1}, {M3, 1}, {M5, 0.5f}, {M5, 0.5f},
    // 这片混沌 柳暗
    {M6, 0.5f}, {M5, 1}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f},
    // 花明 一村一村一村
    {M2, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f},
    // 一村又一村
    {M2, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {L7, 0.5f}, {M3, 1}, {M1, 1},
    // 不能理顺我
    {M2, 1}, {M3, 1}, {M2, 1}, {M3, 0.5f}, {M3, 0.5f},
    // 自己的疑问
    {M2, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M5, 1.5f}, {M3, 1},
    // 病榻上传来了我微
    {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M2, 0.5f},
    // 弱的呼吸声，没有啥大能
    {M1, 0.5f}, {M3, 0.5f}, {M1, 0.125f}, {M3, 0.125f}, {M5, 0.25f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 耐，也无法忍受失
    {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f},
    // 败 杀死人的从
    {M1, 0.5f}, {P0, 0.5f}, {P0, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 来不是挫折而是期
    {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f},
    // 待 遏制住我发
    {M2, 1}, {P0, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M6, 0.5f}, {M3, 0.5f}, {M5, 0.5f},
    // 疯 也没有让我太痛
    {M5, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f},
    // 快 噢 我茅塞顿
    {M2, 1}, {P0, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 开 原来是自尊心在作
    {M2, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M3, 0.25f}, {M3, 0.25f}, {M3, 0.5f}, {M2, 0.5f},
    // 怪 漏了一拍 我只能
    {M1, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M5, 0.5f},
    // 每天都把自己搞得很晕
    {M6, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M3, 0.25f}, {M5, 0.25f}, {M3, 0.5f}, {M5, 1},
    // 坏事降临头上也不清醒
    {M6, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M3, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f},
    // 可笼罩在我头顶上的乌云
    {M2, 0.25f}, {M2, 0.25f}, {M2, 0.25f}, {M2, 0.25f}, {M2, 0.25f}, {M1, 0.25f}, {M1, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M5, 1},
    // 紧跟着逃不出她的手心
    {M2, 0.25f}, {M2, 0.25f}, {M2, 0.5f}, {M2, 0.25f}, {M1, 0.25f}, {M1, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M3, 0.5f}, {M5, 1},
    // 呀 我的愁绪千
    {M1, 1}, {P0, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f},
    // 丝万缕挥之不去
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.25f}, {M3, 0.25f}, {M2, 0.5f},
    // 闭上眼眼眶里下了场雨
    {M2, 0.25f}, {M2, 0.25f}, {M2, 0.5f}, {M2, 0.5f}, {M3, 0.25f}, {M2, 0.25f}, {M2, 0.25f}, {M2, 0.25f}, {M3, 0.5f}, {M2, 1},
    // 坠入了梦乡也弄湿了枕巾
    {M5, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.25f}, {M6, 0.25f}, {M3, 0.5f}, {M5, 1},
    // 梦把我拽到个好地方 摇
    {M6, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M3, 0.25f}, {M3, 0.25f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M5, 0.5f},
    // 摇晃晃到莫愁乡
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 1},
    // 炊烟漫过青瓦黛砖
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M1, 0.5f},
    // 脚印伴随着微光 这
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f},
    // 伤 不痛不痒 她
    {M1, 1.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f},
    // 讲 你在说谎 都是
    {M1, 1}, {P0, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M1, 0.25f}, {M1, 0.25f},
    // 装的坚强 你眼里
    {M3, 0.5f}, {M2, 1}, {M3, 0.5f}, {M2, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f},
    // 带着泪光 我
    {M3, 0.5f}, {M2, 1}, {M3, 0.5f}, {M2, 1.5f}, {M1, 0.5f},
    // 诉愁肠向莫愁乡 把
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M5, 0.5f},
    // 叹息吹成地上霜
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 1},
    // 明明快崩溃了 却还
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M1, 0.5f}, {M1, 0.5f},
    // 要乔装无关痛痒 墙
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f},
    // 边拨浪鼓叮当响 往
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f},
    // 我手心上放块儿糖
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 娃儿莫慌 姥姥
    {M3, 0.5f}, {M2, 1}, {M3, 0.5f}, {M2, 1}, {M1, 0.5f}, {M1, 0.5f},
    // 在边上 放声
    {M3, 0.5f}, {M2, 1}, {M3, 0.5f}, {M2, 1}, {M1, 0.5f}, {M1, 0.5f},
    // 大哭吧 这里没人
    {M5, 0.5f}, {P0, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 会笑话你
    {M3, 0.5f}, {M2, 1}, {M1, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 1},
    // 人生大事莫过生死
    {M5, 0.5f}, {M5, 0.5f}, {M6, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f},
    // 其他都不要紧
    {M5, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M1, 1},
    // 现实太要命了 让我
    {M1, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f},
    // 时刻想要逃离
    {M1, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 1}, {M2, 0.5f}, {M3, 0.5f},
    // 可是 可是 转眼
    {M2, 0.5f}, {M2, 0.5f}, {M1, 0.5f}, {M2, 1}, {P0, 0.5f}, {M2, 0.5f}, {M2, 0.5f},
    // 间被闹钟叫醒
    {M1, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 1}, {M1, 1},
    // 累了就出门溜达溜达
    {M6, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f},
    // 放心吧兜里够花够花
    {M6, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f},
    // 我骗她是国企 我实际是牛马
    {M1, 0.25f}, {M1, 0.25f}, {M1, 0.25f}, {M1, 0.25f}, {M1, 0.25f}, {M1, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M3, 0.25f}, {M3, 0.5f}, {M3, 0.25f}, {M2, 0.5f}, {M3, 0.5f},
    // 早九晚八五天年假
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 幺儿你在那边缺啥少啥
    {M6, 0.5f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.25f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M5, 0.5f},
    // 有事没事都打个电话
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M3, 0.25f}, {M3, 0.25f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 幺儿你到底啥时候回家
    {M2, 0.25f}, {M2, 0.25f}, {M2, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M2, 0.25f}, {M3, 0.25f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 0.5f},
    // 姥姥种的石榴开花了
    {M2, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M1, 0.25f}, {M1, 0.25f}, {M3, 0.5f}, {M3, 0.5f}, {M1, 0.5f},
    // 几度梦回莫愁乡 回
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M5, 0.5f},
    // 回梦的都不重样
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 1},
    // 纸飞机掠过操场后
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M1, 0.5f}, {M3, 0.5f},
    // 铁了心的要流浪
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 哪管前路几多长
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 偏信远方有朝阳
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 却输岁月半柱香
    {M3, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M2, 1},
    // 人走茶凉
    {M3, 0.5f}, {M2, 1}, {M3, 0.5f}, {M2, 1}, {M1, 1},
    // 几度梦回莫愁乡 旧
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M5, 0.5f},
    // 木船飘向芦苇荡
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M5, 0.5f}, {M5, 1},
    // 载着未寄出的信 还
    {M6, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M1, 0.5f},
    //有糖纸染上的香
    {M1, 0.5f}, {M1, 0.5f}, {M1, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 摇 摇摇晃晃
    {M1, 1}, {P0, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 飘 飘飘荡荡
    {M1, 1}, {P0, 0.5f}, {M5, 0.5f}, {M5, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f},
    // 娃儿抬头望
    {M3, 0.5f}, {M2, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M3, 0.5f}, {M2, 1.5f},
    // 姥姥在天上
    {M1, 0.5f}, {M3, 0.5f}, {M2, 0.5f}, {M3, 0.5f}, {M2, 1}, {M1, 1},
};

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

/* LED 輪流點亮變數 */
typedef enum
{
  LED_STATE_RED = 0,
  LED_STATE_GREEN = 1,
  LED_STATE_BLUE = 2
} LED_State_t;
static LED_State_t current_led_state = LED_STATE_RED;
static uint32_t last_led_tick = 0;      // LED 狀態更新時間戳

/* 音樂播放狀態 */
static uint8_t music_playing = 0;
static size_t music_index = 0;
static float noteDurationMs = 0.0f;
static uint32_t music_next_tick = 0;

uint32_t TIM_GetCounterFreq(TIM_HandleTypeDef *htim) {
    uint32_t timer_clock;
    // 高级定时器是APB2
    if (htim->Instance == TIM1) {
        timer_clock = HAL_RCC_GetPCLK2Freq();
        // 如果APB分频不为1，定时器时钟会翻倍
        if (HAL_RCC_GetPCLK2Freq() != (HAL_RCC_GetHCLKFreq() / 1)) {
            timer_clock *= 2;
        }
    } else {
    	// 其他定时器是APB1
        timer_clock = HAL_RCC_GetPCLK1Freq();
        // 如果APB分频不为1，定时器时钟会翻倍
        if (HAL_RCC_GetPCLK1Freq() != (HAL_RCC_GetHCLKFreq() / 1)) {
            timer_clock *= 2;
        }
    }

    uint32_t prescaler = htim->Instance->PSC;
    return timer_clock / (prescaler + 1);
}

void init_music(void) {
  HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
  music_playing = 0;
  music_index = 0;
  music_next_tick = 0;
  noteDurationMs = 1000.0f * 60.0f / 132.0f;
}

void start_music(void)
{
  if (!music_playing)
  {
    music_playing = 1;
    music_index = 0;
    music_next_tick = 0;
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
  }
}

void stop_music(void)
{
  music_playing = 0;
  music_index = 0;
  music_next_tick = 0;
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 0);
  HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
}

void play_music(void) {
  if (!music_playing) {
    return;
  }

  uint32_t now = HAL_GetTick();
  uint32_t timFrequency = TIM_GetCounterFreq(&htim4);

  if (music_next_tick == 0 || now >= music_next_tick)
  {
    const Bate bate = MoChouXiang[music_index];

    if (bate.frequency == P0)
    {
      __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 0);
    }
    else
    {
      uint32_t arr = timFrequency / bate.frequency;
      if (arr == 0) {
        arr = 1;
      }
      __HAL_TIM_SET_AUTORELOAD(&htim4, arr);
      __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, arr / 5);
      __HAL_TIM_SetCounter(&htim4, 0);
    }

    music_next_tick = now + (uint32_t)(bate.period * noteDurationMs);
    music_index++;
    if (music_index >= (sizeof(MoChouXiang) / sizeof(MoChouXiang[0])))
    {
      music_index = 0;
    }
  }
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
  OLED_PrintString(0, 0, " WATER REMINDER ", &font16x16, OLED_COLOR_NORMAL);
  OLED_PrintString(0, 16, "STATUS: IDLE    ", &font16x16, OLED_COLOR_NORMAL);
  char buf[32];
  // sprintf(buf, "TIMER:  %02d mins", set_time_minutes);
  //OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
  sprintf(buf, "TIMER: %02d:%02dmins", set_time_minutes, 0);
  OLED_PrintString(0, 32, buf, &font16x16, OLED_COLOR_NORMAL);
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
      /* 逆時針旋轉，減少時間 */
      if (set_time_minutes > 10)
      {
        set_time_minutes--;
      }
    }
    else
    {
      /* 順時針旋轉，增加時間 */
      if (set_time_minutes < 60)
      {
        set_time_minutes++;
      }
    }
    last_encoder_count = current_count;
  }
}

/* 按鈕處理 */
void button_task(void)
{
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
  OLED_PrintString(0, 0, " WATER REMINDER ", &font16x16, OLED_COLOR_NORMAL);
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