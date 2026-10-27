/*
 * pwm.h
 *
 *  Created on: 15 Jun 2026
 *      Author: ivanp
 */

#ifndef INC_PWM_H_
#define INC_PWM_H_

#include "main.h"
#include "adc_FOC.h"
#include "cordic.h"


void PWM_Init(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim8);

void pwm_set_duty_percent(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t pct);

void setCH4duty(TIM_HandleTypeDef *htim, uint32_t pwm4_duty);

#endif /* INC_PWM_H_ */
