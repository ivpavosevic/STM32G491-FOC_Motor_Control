/*
 * uart.c
 *
 * Purpose: UART communication
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "uart.h"
#include "adc.h"


static uint8_t  rx_byte;
static char  rx_line[RX_LINE_MAX];
static volatile uint32_t rx_len = 0;
static volatile uint8_t rx_line_ready = 0;

static UART_HandleTypeDef *s_huart;

static void UART_Start_RX_IT(void)
{
    if (s_huart != NULL)
    {
        HAL_UART_Receive_IT(s_huart, (uint8_t*)&rx_byte, 1);
    }
}

void UART_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    rx_len = 0;
    rx_line_ready = 0;

    UART_Start_RX_IT();
}

void UART_GetLine(char *dst, uint32_t max_len)
{
    __disable_irq();
    if (rx_line_ready)
    {
        strncpy(dst, rx_line, max_len);
        dst[max_len - 1U] = '\0';
        rx_line_ready = 0;
    }
    else
    {
        if (max_len > 0) dst[0] = '\0';
    }
    __enable_irq();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == s_huart)
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

		HAL_UART_Receive_IT(s_huart, &rx_byte, 1);
	}
}

uint8_t UART_IsLineReady(void)
{
    return rx_line_ready;
}

/* Function used to send key variables via UART + DMA to Matlab live plot script */
void UART_Logger(){
	// Check if DMA is busy
	if (HAL_DMA_GetState(s_huart->hdmatx) == HAL_DMA_STATE_BUSY) {
	    return;
	}
	// Send data via DMA
	static char buf1[60];
	float f_Id = get_Id();
	float f_Iq = get_Iq();
    int n1 = snprintf(buf1, sizeof(buf1), "%.3f, %.3f\n ", f_Id, f_Iq);
    HAL_UART_Transmit_DMA(s_huart, (uint8_t *)buf1, n1);

}






