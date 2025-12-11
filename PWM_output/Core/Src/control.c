/*
 * control.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "main.h"
#include "pwm.h"
#include "uart.h"

TIM_HandleTypeDef tim1;
TIM_HandleTypeDef tim2;
TIM_HandleTypeDef tim3;


void Control_Init(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2, TIM_HandleTypeDef *htim3){
	tim1 = htim1;
	tim2 = htim2;
	tim3 = htim3;
}


void process_line(char *line)
{
    if (strncmp(line, "pwm", 3) == 0) {
        line += 3;
        while (*line == ' ' || *line == '\t') line++;
        int val = (*line) ? atoi(line) : -1;
        if (val < 0)   val = 0;
        if (val > 100) val = 100;

        pwm_set_duty_percent(&htim2, TIM_CHANNEL_1, (uint32_t)val);

        char ack[32];
        int n = snprintf(ack, sizeof(ack), "OK pwm=%d%%\r\n", val);
        UART_Transmit((uint8_t*)ack, n);

        return;
    }

    const char *err = "ERR: use 'pwm <0..100>'\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)err, strlen(err), HAL_MAX_DELAY);
}

