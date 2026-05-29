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
#include "adc.h"

/* Return codes */
#define CORDIC_SIN_OK 0
#define CORDIC_SIN_ERROR -1

#define SQRT3 1.73205080757f

#define TWO_PI (2.0f * (float)M_PI)
/* Q31 format: full scale = 2^31 */
#define Q31_SCALE 2147483648.0f


#define INV_PI_SCALE 2147483648.0f/3.14159265358979323846f





void calculateInvClarke(float *Ua, float *Ub, float *Uc, float Ualpha, float Ubeta);

void calculateClarke(float Ia, float Ib, float Ic, float *Ialpha, float *Ibeta);

void calculatePark(float Ialpha, float Ibeta, float theta, float *Iq, float *Id, float sin_t, float cos_t);

void calculateInvPark(float *Ualpha, float *Ubeta, float theta, float Uq, float Ud, float sin_t, float cos_t, drive_state_t drive_state);

void CORDIC_Change_Constant_Angle(uint8_t angle_flag);

int CORDIC_Sin_Init(CORDIC_HandleTypeDef *hcordic_ptr, float pwm_freq_hz);

void CORDIC_Sin_SetFrequency(float freq_hz);

void CORDIC_CalculateSinCos(float theta, float *sin_t, float *cost_t);

void CORDIC_Update_Angle(void);

float CORDIC_Get_Angle(void);

int CORDIC_Sin_Get3Phase(float *sin_a, float *sin_b, float *sin_c);

float fast_sqrt(float x);

#endif /* INC_CORDIC_SIN_H_ */
