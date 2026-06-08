#include "rs232.h"

uint8_t rs232Buffer[RS232_RX_BUF_SIZE];
uint8_t lineReceived = 0;

void RS232_Init(void) {
	// 启动UART中断接收
	HAL_UART_Receive_IT(&RS232_UART, rs232Buffer, 1);
}

void RS232_Puts(const char *str) {
	HAL_UART_Transmit(&RS232_UART, (uint8_t*) str, strlen(str), 0xFFFF);
}

void RS232_Printf(const char *format, ...) {
	char buf[128];
	va_list args;
	va_start(args, format);
	int len = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	HAL_UART_Transmit(&RS232_UART, (uint8_t*) buf, len, 0xFFFF);
}

uint8_t RS232_GetLine(char *buf, uint16_t size) {
	if (!lineReceived)
		return 0;
	lineReceived = 0;
	memcpy(buf, rs232Buffer, size);
	return 1;
}

#ifdef __GNUC__
int _write(int file, char *ptr, int len) {
	HAL_UART_Transmit(&RS232_UART, (uint8_t*) ptr, len, 0xFFFF);
	return len;
}
#endif

void RS232_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == &RS232_UART) {
        // 处理接收到的字符
        static uint16_t rxIndex = 0;
        uint8_t rxData = rs232Buffer[0];
        
        if (rxData == '\n') {
            // 处理回车和换行符
            rs232Buffer[rxIndex] = '\0';
            if (rxIndex > 0 && rs232Buffer[rxIndex-1] == '\r') {
                rs232Buffer[rxIndex-1] = '\0';
            }
            lineReceived = 1;
            rxIndex = 0;
        } else if (rxIndex < RS232_RX_BUF_SIZE - 1) {
            rs232Buffer[rxIndex++] = rxData;
        }
        
        // 继续接收下一个字符
        HAL_UART_Receive_IT(&RS232_UART, rs232Buffer, 1);
    }
}
