/*
 * pwm.h
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#ifndef INC_PWM_H_
#define INC_PWM_H_

#include "main.h"

/**
 * @brief Set constant PWM duty cycle (for testing)
 * @param htim Timer handle
 * @param channel Timer channel
 * @param pct Duty cycle percentage (0-100)
 */
void pwm_set_duty_percent(TIM_HandleTypeDef *htim, uint32_t channel,
                          uint32_t pct);

/**
 * @brief Set motor electrical frequency (controls rotation speed)
 * @param freq_hz Electrical frequency in Hz
 *        Motor mechanical speed = freq_hz / pole_pairs
 *        Example: 50 Hz with 4 pole pairs = 12.5 RPS = 750 RPM
 */
void PWM_SetMotorFrequency(float freq_hz);

/**
 * @brief Get current motor electrical frequency
 * @retval Frequency in Hz
 */
float PWM_GetMotorFrequency(void);

/**
 * @brief Set modulation index (voltage amplitude)
 * @param m Modulation index 0.0 to 1.0 (1.0 = full amplitude)
 */
void PWM_SetModulationIndex(float m);

/**
 * @brief Get current modulation index
 * @retval Modulation index
 */
float PWM_GetModulationIndex(void);

/* Debug functions */
uint16_t PWM_GetCCR1_1st(void);
uint16_t PWM_GetCCR1_2nd(void);

#endif /* INC_PWM_H_ */
