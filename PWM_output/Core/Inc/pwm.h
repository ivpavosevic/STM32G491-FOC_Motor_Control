/*
 * pwm.h
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#ifndef INC_PWM_H_
#define INC_PWM_H_
#include "main.h"

void pwm_set_duty_percent(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t pct);

#endif /* INC_PWM_H_ */
