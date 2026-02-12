/*
 * cordic_sin.h
 *
 * CORDIC-based sine/cosine calculation for motor control
 * Supports variable angle increment for speed control
 *
 *  Created on: Jan 29, 2026
 */

#ifndef INC_CORDIC_SIN_H_
#define INC_CORDIC_SIN_H_

#include "main.h"

/* Return codes */
#define CORDIC_SIN_OK 0
#define CORDIC_SIN_ERROR -1

void CORDIC_Change_Constant_Angle(uint8_t angle_flag);

int CORDIC_Sin_Init(CORDIC_HandleTypeDef *hcordic_ptr, float pwm_freq_hz);

void CORDIC_Sin_SetFrequency(float freq_hz);

int32_t CORDIC_Get_Angle(void);

int CORDIC_Sin_Get3Phase(float *sin_a, float *sin_b, float *sin_c);


#endif /* INC_CORDIC_SIN_H_ */
