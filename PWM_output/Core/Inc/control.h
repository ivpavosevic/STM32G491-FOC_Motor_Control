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

#define PI_IQ_OUT_MAX 1
#define PI_IQ_OUT_MIN -1
#define PI_ID_OUT_MAX 1
#define PI_ID_OUT_MIN -1

#define POLE_PAIRS 8


extern volatile uint8_t new_Hall_meas_flag;
extern volatile float new_Hall_meas_angle;
extern volatile float new_Hall_meas_speed;

typedef struct{
	float Kp;
	float Ki;
	float sum_err;
	float sum_err_limit;
	float out_min;
	float out_max;
	float dt;
} PI_reg_t;

void Control_Init(TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel, UART_HandleTypeDef *huart);

void PI_Init_q(PI_reg_t *pi_iq);

void PI_Init_d(PI_reg_t *pi_id);

void Setup_Init(input_params *ip);

float Control_PI_reg(PI_reg_t *pi_x, float err);

void process_line(char *line);

uint32_t readHall(void);

uint32_t readAngle(void);


#endif /* INC_CONTROL_H_ */
