//
// Created by hemeng on 25-4-19.
//

#include "app_uart.h"



extern uint8_t uartRxIndex;
extern char uartRxBuffer[UART_RX_BUFFER_SIZE];
extern uint8_t uartReceiveByte;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // 存储接收到的字节
        if (uartRxIndex < UART_RX_BUFFER_SIZE - 1) {
            uartRxBuffer[uartRxIndex++] = uartReceiveByte;
            // 检查结束符
            if (uartReceiveByte == '\n' || uartReceiveByte == '\r') {
                uartRxBuffer[uartRxIndex - 1] = '\0'; // 替换结束符为字符串结束符
                // 处理命令
                if (strncmp(uartRxBuffer, "reboot", uartRxIndex) == 0) {
                    printf("Rebooting...\n");
                    NVIC_SystemReset();
                }
                if (strncmp(uartRxBuffer, "time", uartRxIndex) == 0) {
                    DS3231_TimeType rtcTime;
                    DS3231_GetTime(&rtcTime);
                    printf("Time:20%02d-%02d-%02d %02d:%02d:%02d\n",
                           rtcTime.year, rtcTime.moon, rtcTime.day,
                           rtcTime.hour, rtcTime.min, rtcTime.sec);
                } else {
                    printf("%.*s\r\n", uartRxIndex, uartRxBuffer);
                }
                uartRxIndex = 0; // 重置索引
            }
        } else {
            printf("Buffer overflow. Clearing buffer.\n");
            memset(uartRxBuffer, 0, UART_RX_BUFFER_SIZE);
            uartRxIndex = 0;
        }
        // 继续接收下一个字节
        HAL_UART_Receive_IT(huart, &uartReceiveByte, 1);
    }
}
