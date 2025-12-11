/*
 * uart.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "uart.h"

static volatile uint8_t  rx_byte;
static char  rx_line[RX_LINE_MAX];
static volatile uint32_t rx_len = 0;
volatile uint8_t  rx_line_ready = 0;

static UART_HandleTypeDef *s_huart;

void UART_Init(UART_HandleTypeDef *huart){
	s_uart = huart;
	HAL_UART_Receive_IT(&huart, (uint8_t*)&rx_byte, 1);
}


void UART_Transmit(uint8_t *string, n){
	HAL_UART_Transmit(&s_uart, (uint8_t*)string, n, HAL_MAX_DELAY);
}

void HAL_UART_RxCpltCallback()
{

	uint8_t b = rx_byte;

	if (b == '\r' || b == '\n') {
		if (rx_len < RX_LINE_MAX) rx_line[rx_len] = '\0';
		rx_line_ready = 1;
		rx_len = 0;
	} else {
		if (rx_len < RX_LINE_MAX - 1) rx_line[rx_len++] = (char)b;
		else rx_len = 0;
	}
	HAL_UART_Receive_IT(&s_huart, &rx_byte, 1);
}
