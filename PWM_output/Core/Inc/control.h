/*
 * control.h
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#ifndef INC_CONTROL_H_
#define INC_CONTROL_H_
#include "main.h"
void Control_Init(TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel, UART_HandleTypeDef *huart, ADC_HandleTypeDef *hadc);

void process_line(char *line);

#endif /* INC_CONTROL_H_ */
