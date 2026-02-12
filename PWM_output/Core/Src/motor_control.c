/*
 * motor_control.c
 *
 *  Created on: Feb 3, 2026
 *      Author: ivanp
 */
#include "motor_control.h"
#include <math.h>


void calculateInvClarke(float *Ua, float *Ub, float *Uc, float Ualpha, float Ubeta){
	*Ua = Ualpha;
	*Ub = 1/2*(-Ualpha + sqrt(3)*Ubeta);
	*Uc = 1/2*(-Ualpha - sqrt(3)*Ubeta);
	return;
}

void calculateClarke(float Ia, float Ib, float Ic, float *Ialpha, float *Ibeta){
	*Ialpha = 1/3*(2*Ia - Ib - Ic);
	*Ibeta = 1/3*(sqrt(3)*Ib - sqrt(3)*Ic);
	return;
}

void calculatePark(float Ialpha, float Ibeta, float *Iq, float *Id){
	float cos_t = cosf(30);
	float sin_t = sinf(30);
	*Id =  Ialpha * cos_t + Ibeta * sin_t;
	*Iq = -Ialpha * sin_t + Ibeta * cos_t;
	return;
}

void calculateInvPark(float *Ualpha, float *Ubeta, float Uq, float Ud){
	float cos_t = cosf(30);
	float sin_t = sinf(30);
	*Ualpha = Ud * cos_t - Uq * sin_t;
	*Ubeta  = Ud * sin_t + Uq * cos_t;
	return;
}
