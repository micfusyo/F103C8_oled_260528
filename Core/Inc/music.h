#ifndef __MUSIC_H
#define __MUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 音符频率定义 */
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

/* 变化音频率定义 */
#define LS1 277  // 低音升C/降D
#define LS2 311  // 低音升D/降E
#define LS4 370  // 低音升F/降G
#define LS5 415  // 低音升G/降A
#define MS1 554  // 中音升C/降D
#define MS2 622  // 中音升D/降E
#define MS4 740  // 中音升F/降G
#define MS5 831  // 中音升G/降A
#define HS1 1109 // 高音升C/降D
#define HS2 1245 // 高音升D/降E

/* 音符结构体 */
typedef struct
{
    uint16_t frequency;
    float period;
} Bate;

/* 函数声明 */
void init_music(void);
void start_music(void);
void stop_music(void);
void play_music(void);
uint32_t TIM_GetCounterFreq(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif /* __MUSIC_H */
