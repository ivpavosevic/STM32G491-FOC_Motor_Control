/*
 * uart.h
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#ifndef INC_UART_H_
#define INC_UART_H_
#include "main.h"
#define RX_LINE_MAX 64

void UART_Init(UART_HandleTypeDef *huart);
void UART_GetLine(char *dst, uint32_t max_len);
uint8_t UART_IsLineReady(void);



#endif /* INC_UART_H_ */
