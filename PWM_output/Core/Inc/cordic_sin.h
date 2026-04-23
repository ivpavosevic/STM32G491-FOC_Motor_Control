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

typedef struct {
    float i_d;
    float i_q;
} foc_i_dq_t;


typedef struct {
    float u_d;
    float u_q;
} foc_u_dq_t;

typedef struct {
    float i_alfa;
    float i_beta;
} foc_i_alfabeta_t;

typedef struct {
    float u_alfa;
    float u_beta;
} foc_u_alfabeta_t;



void calculateInvClarke(float *Ua, float *Ub, float *Uc, float Ualpha, float Ubeta);

void calculateClarke(float Ia, float Ib, float Ic, float *Ialpha, float *Ibeta);

void calculatePark(float Ialpha, float Ibeta, float theta, float *Iq, float *Id);

void calculateInvPark(float *Ualpha, float *Ubeta, float theta, float Uq, float Ud);

void CORDIC_Change_Constant_Angle(uint8_t angle_flag);

int CORDIC_Sin_Init(CORDIC_HandleTypeDef *hcordic_ptr, float pwm_freq_hz);

void CORDIC_Sin_SetFrequency(float freq_hz);

float CORDIC_Get_Angle(void);

int CORDIC_Sin_Get3Phase(float *sin_a, float *sin_b, float *sin_c);


#endif /* INC_CORDIC_SIN_H_ */
