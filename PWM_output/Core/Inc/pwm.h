/*
 * pwm.h
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#ifndef INC_PWM_H_
#define INC_PWM_H_

#include "main.h"
#include "cordic_sin.h"
#include "control.h"
#include "uart.h"
#include "adc.h"

/**
 * @brief Set constant PWM duty cycle (for testing)
 * @param htim Timer handle
 * @param channel Timer channel
 * @param pct Duty cycle percentage (0-100)
 */
void pwm_set_duty_percent(TIM_HandleTypeDef *htim, uint32_t channel,
                          uint32_t pct);

void setCH4duty(TIM_HandleTypeDef *htim, uint32_t pwm4_duty);

#endif /* INC_PWM_H_ */
