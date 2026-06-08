// 注意：這個版本有把printf加到rs232文件裡，可直接使用printf做Debug用
// 但沒做發送完成確認，發送和發送之間最好要自行預留時間，以免後面發送資料被覆蓋

#ifndef __RS232_H
#define __RS232_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "usart.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define RS232_UART huart2
#define RS232_RX_BUF_SIZE 128
extern uint8_t rs232Buffer[];
extern uint8_t lineReceived;

void RS232_Init(void);
void RS232_Puts(const char *str);
void RS232_Printf(const char *format, ...);
uint8_t RS232_GetLine(char *buf, uint16_t size);
void RS232_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

#ifdef __cplusplus
}
#endif

#endif /* __RS232_H */
