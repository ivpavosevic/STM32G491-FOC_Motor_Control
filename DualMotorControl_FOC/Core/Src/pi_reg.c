/*
 * pi_reg.c
 *
 *  Created on: 16 Jun 2026
 *      Author: ivanp
 */
#include "pi_reg.h"

/* ============ Private variables ============ */

/* ============ Private functions ============ */
/* d-axis current loop gains + limits (regulates Id, outputs Ud) */
void PI_Init_d(PI_reg_t *pi_id){
	pi_id->Ki = 5.0f; // 5
	pi_id->Kp = 0.5f; // 0.5
	pi_id->out_min = -3.0f; // -3
	pi_id->out_max = 3.0f; // 3
	pi_id->sum_err = 0.0f;
	pi_id->sum_err_limit = pi_id->out_max / pi_id->Ki;   // can't exceed out_max
	pi_id->dt = 0.0f;
}

/* q-axis current loop gains + limits (regulates Iq, outputs Uq - the torque-producing axis). */
void PI_Init_q(PI_reg_t *pi_iq){
	pi_iq->Ki = 7.0f; // 5
	pi_iq->Kp = 0.9f; // 1
	pi_iq->out_min = -5.0f; // -5
	pi_iq->out_max = 5.0f; // -5
	pi_iq->sum_err = 0.0f;
	pi_iq->sum_err_limit = pi_iq->out_max / pi_iq->Ki;   // can't exceed out_max
	pi_iq->dt = 0.0f;
}


/* Outer speed loop gains + limits (omega error -> Iq_ref).
 * NOTE: currently not used */
void PI_Init_omega(PI_reg_t *pi_omega){
	pi_omega->Ki = 0.05f;
	pi_omega->Kp = 0.008f;
	pi_omega->out_min = -0.7f;
	pi_omega->out_max = 0.7f;
	pi_omega->sum_err = 0.0f;
	pi_omega->sum_err_limit = pi_omega->out_max / pi_omega->Ki;   // can't exceed out_max
	pi_omega->dt = 0.0f;
}

/* PI regulator
 * Input:  pi_x (gains + running state), err (setpoint - measured), dt_s (loop period)
 * Output: returns the  U_x */
float Control_PI_reg(PI_reg_t *pi_x, float err, float dt_s){
	float U_x;

	pi_x->sum_err += err * dt_s;              // integrate the error over time

	// Clamp integrator (standard anti-windup)
	if(pi_x->sum_err > pi_x->sum_err_limit)
		pi_x->sum_err = pi_x->sum_err_limit;
	if(pi_x->sum_err < -pi_x->sum_err_limit)
		pi_x->sum_err = -pi_x->sum_err_limit;

	U_x = pi_x->Kp * err + pi_x->Ki * pi_x->sum_err;

	if(U_x >= pi_x->out_max){
		U_x = pi_x->out_max;                  // output saturated high
		pi_x->sum_err -= err * dt_s;          // undo integration
		return U_x;
	}
	if(U_x <= pi_x->out_min){
		U_x = pi_x->out_min;                  // output saturated low
		pi_x->sum_err -= err * dt_s;          // undo integration
		return U_x;
	}
	return U_x;
}
