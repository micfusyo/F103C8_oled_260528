#ifndef __CTRL_H
#define __CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void update_oled_idle(void);
void update_oled_counting(void);
void update_oled_alarm(void);
void encoder_task(void);
void button_task(void);
void timer_task(void);
void alarm_task(void);
void draw_progress_bar(uint16_t x, uint16_t y, uint16_t width, uint16_t current, uint16_t total);
void setup(void);
void loop(void);

#endif /* __CTRL_H */