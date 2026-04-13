/*
 * control.h
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#ifndef INC_CONTROL_H_
#define INC_CONTROL_H_
#include "main.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define TEST_SIZE 48

extern volatile uint8_t new_Hall_meas_flag;
extern volatile float new_Hall_meas_angle;

void Control_Init(TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel, UART_HandleTypeDef *huart);

void process_line(char *line);

uint32_t readHall(void);

uint32_t readAngle(void);


#endif /* INC_CONTROL_H_ */
