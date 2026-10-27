/*
 * cordic.h
 *
 *  Created on: 16 Jun 2026
 *      Author: ivanp
 */
#ifndef INC_CORDIC_H_
#define INC_CORDIC_H_

#include "main.h"

#define CORDIC_SIN_OK 0
#define CORDIC_SIN_ERROR -1

/* Q31 format: full scale = 2^31 */
#define Q31_SCALE 2147483648.0f
#define SQRT3 1.73205080757f
#define INV_PI_SCALE 2147483648.0f/3.14159265358979323846f

#define PHASE_OFFSET_120_UQ31 ((uint32_t)0x55555555u) /* +120° */
#define PHASE_OFFSET_240_UQ31 ((uint32_t)0xAAAAAAABu) /* +240° == -120° (signed) */

void CORDIC_Sin_SetFrequency(void);
void CORDIC_Update_Angle(void);
int CORDIC_Init(CORDIC_HandleTypeDef *hcordic_ptr);

void calculateInvClarke(float *Ua, float *Ub, float *Uc, float Ualpha, float Ubeta);
void calculateClarke(float Ia, float Ib, float Ic, float *Ialpha, float *Ibeta);

void calculatePark(float Ialpha, float Ibeta, float theta, float *Iq, float *Id, float sin_t, float cos_t);
void calculateInvPark(float *Ualpha, float *Ubeta, float theta, float Uq, float Ud, float sin_t, float cos_t, drive_state_t drive_state);

void CORDIC_CalculateSinCos(float theta, float *sin_t, float *cost_t);

#endif /* INC_CORDIC_H_ */
