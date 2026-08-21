#include "music.h"
#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef htim4;  // 蜂鸣器使用的定时器

/* 音乐数据：給愛麗絲 (Für Elise) - 贝多芬 */
const Bate FurElise[] = {
    /* === 第一段 (A段): 著名的E-D#交替开头 + A小调旋律 === */
    // E5 D#5 E5 D#5 E5 B4 D5 C5
    {M3, 1}, {MS2, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {L7, 1}, {M2, 1}, {M1, 1},
    // A4 (长音)
    {L6, 2}, {P0, 1},
    // C5 E5 A5
    {M1, 1}, {M3, 1}, {L6, 2},
    // B4 (长音)
    {L7, 2}, {P0, 1},
    // E5 G#5 B5
    {M3, 1}, {LS5, 1}, {L7, 2},
    // C5 E5 E5 D#5 E5 D#5 E5 B4 D5 C5
    {M1, 1}, {M3, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {L7, 1}, {M2, 1}, {M1, 1},
    // A4 (长音)
    {L6, 2}, {P0, 1},
    // C5 E5 A5
    {M1, 1}, {M3, 1}, {L6, 2},
    // B4 (长音)
    {L7, 2}, {P0, 1},
    // E5 C5 B4 A4
    {M3, 1}, {M1, 1}, {L7, 1}, {L6, 2},

    /* === 第二段: 升高部分 === */
    // E5 D#5 E5 D#5 E5 B4 D5 C5
    {M3, 1}, {MS2, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {L7, 1}, {M2, 1}, {M1, 1},
    // A4 (长音)
    {L6, 2}, {P0, 1},
    // C5 E5 A5
    {M1, 1}, {M3, 1}, {L6, 2},
    // B4 (长音)
    {L7, 2}, {P0, 1},
    // E5 G#5 B5
    {M3, 1}, {LS5, 1}, {L7, 2},
    // C5 E5 E5 D#5 E5 D#5 E5 B4 D5 C5
    {M1, 1}, {M3, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {L7, 1}, {M2, 1}, {M1, 1},
    // A4 (长音)
    {L6, 2}, {P0, 1},
    // C5 E5 A5
    {M1, 1}, {M3, 1}, {L6, 2},
    // B4 (长音)
    {L7, 2}, {P0, 1},
    // E5 C5 B4 A4
    {M3, 1}, {M1, 1}, {L7, 1}, {L6, 2},

    /* === 第三段 (B段): 转到F大调 === */
    // E5 C5 B4 A4 E5 D#5 E5 D#5 E5 A4
    {M3, 1}, {M1, 1}, {L7, 1}, {L6, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {L6, 1},
    // B4 C5 D5 C5 B4 A4
    {L7, 1}, {M1, 1}, {M2, 1}, {M1, 1}, {L7, 1}, {L6, 1},
    // E5 C5 B4 A4 E5 D#5 E5 D#5 E5 B4 D5 C5
    {M3, 1}, {M1, 1}, {L7, 1}, {L6, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {MS2, 1}, {M3, 1}, {L7, 1}, {M2, 1}, {M1, 1},
    // A4 (长音)
    {L6, 2}, {P0, 1},
    // C5 E5 A5
    {M1, 1}, {M3, 1}, {L6, 2},
    // B4 (长音)
    {L7, 2}, {P0, 1},
    // E5 C5 B4 A4
    {M3, 1}, {M1, 1}, {L7, 1}, {L6, 2},
};

/* 音乐播放状态变量 */
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
    noteDurationMs = 1000.0f * 60.0f / 140.0f;  // Für Elise: ~140 BPM
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
        const Bate bate = FurElise[music_index];

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
        if (music_index >= (sizeof(FurElise) / sizeof(FurElise[0])))
        {
            music_index = 0;
        }
    }
}
