/*
 * cordic.c
 *
 *  Created on: 16 Jun 2026
 *      Author: ivanp
 */
#include "cordic.h"

/* ============ Private variables ============ */

static CORDIC_HandleTypeDef *s_hcordic = NULL;    // CORDIC peripheral handle

static volatile int32_t cordic_angle = 0;         // scratch angle for the open-loop path

/* Open-loop angle sweep (used when running without position feedback) */
volatile uint32_t s_angle_uq31 = 0;      // running angle, unsigned Q31 (wraps cleanly mod 2^32)
volatile float    s_angle_float = 0.0f;  // same angle in radians, just for monitoring
static uint32_t   s_delta_uq31 = 0;      // angle added each PWM period -> sets the sweep speed
static float s_pwm_freq_hz  = 50000.0f;  // PWM carrier frequency
static float s_elec_freq_hz = 10.0f;     // target electrical frequency of the sweep

/* ============ Private functions ============ */
/* angle-format helpers. CORDIC uses Q31: angle/PI * 2^31, so -PI..+PI fills int32 */

/* radians -> Q31 angle for the CORDIC input */
static inline int32_t radians_to_q31(float angle_rad) {
  return (int32_t)((angle_rad / M_PI) * Q31_SCALE);
}

/* Q31 result -> float in -1.0..+1.0 (sin/cos outputs) */
static inline float q31_to_float(int32_t q31_val) {
  return (float)q31_val / Q31_SCALE;
}

/* Q31 angle -> radians in (-PI, PI) */
static inline float q31_to_radians(int32_t q31_val) {
  return (float)q31_val / Q31_SCALE * M_PI;
}


static inline float degree_to_radians(int32_t angle_deg) {
  return ((angle_deg * M_PI) / 180.0);
}

static inline int32_t radians_to_degree(float angle_rad) {
  return (int32_t)((angle_rad * 180.0)/ M_PI);
}

static inline int32_t q31_angle_to_deg(int32_t angle_q31)
{
    /* Koristimo 64-bit da izbjegnemo overflow */
    int64_t tmp = (int64_t)angle_q31 * 180;
    return (int32_t)(tmp >> 31);
}

/* Fold any angle back into the -PI..+PI range */
static inline float wrap_posneg_pi(float a) {
  while (a >  (float)M_PI) a -= TWO_PI;
  while (a < -(float)M_PI) a += TWO_PI;
  return a;
}


/* Works out how far the open-loop angle should step each PWM period.
 * Input:  s_elec_freq_hz, s_pwm_freq_hz (file statics)
 * Output: s_delta_uq31 - angle increment added per ISR tick */
void CORDIC_Sin_SetFrequency(void) {
//  s_elec_freq_hz = freq_hz;

  /* Calculate angle increment per PWM period:
   * delta_rad = 2*PI * freq_hz / pwm_freq_hz
   * delta_q31 = delta_rad / PI * 2^31
   *           = 2 * freq_hz / pwm_freq_hz * 2^31
   *
   * Store in uint32_t so wrap behavior is defined.
   */
  float delta_normalized = 2.0f * s_elec_freq_hz / s_pwm_freq_hz;
  s_delta_uq31 = (uint32_t)(delta_normalized * Q31_SCALE);
}

/* Ran at startup, storing into static variable */
int CORDIC_Init(CORDIC_HandleTypeDef *hcordic_ptr){
	if (hcordic_ptr == NULL) {
		return CORDIC_SIN_ERROR;             // no handle -> bail out
	}
	  s_hcordic = hcordic_ptr;
	  CORDIC_Sin_SetFrequency();
	  return CORDIC_SIN_OK;
}



/**
 * @brief Calculate sine using CORDIC hardware (blocking/polling)
 * @param angle_q31 Angle in Q31 format
 * @param sin_out Pointer to sine result
 * @param cos_out Pointer to cosine result (can be NULL)
 * @retval CORDIC_SIN_OK on success
 */
static int cordic_calculate(int32_t angle_q31, float *sin_out, float *cos_out) {
  CORDIC->WDATA = (uint32_t)angle_q31;           /* kick off the calculation */
  int32_t sin_q31 = (int32_t)CORDIC->RDATA;      /* 1st read = sine   */
  int32_t cos_q31 = (int32_t)CORDIC->RDATA;      /* 2nd read = cosine */

  if (sin_out != NULL) *sin_out = q31_to_float(sin_q31);
  if (cos_out != NULL) *cos_out = q31_to_float(cos_q31);

  return CORDIC_SIN_OK;
}


/* ============ Clarke / Park transforms (and their inverses) ============ */

/* Inverse Clarke - 2-axis (alpha, beta) voltages back to 3 phase voltages.
 * Input:  Ualpha, Ubeta
 * Output: Ua, Ub, Uc  */
void calculateInvClarke(float *Ua, float *Ub, float *Uc, float Ualpha, float Ubeta){
	*Ua = Ualpha;
	*Ub = 0.5f*(-Ualpha + SQRT3*Ubeta);
	*Uc = 0.5f*(-Ualpha - SQRT3*Ubeta);
	return;
}

/* Clarke - 3 phase currents collapsed onto 2 orthogonal axes.
 * Input:  Ia, Ib, Ic (measured)
 * Output: Ialpha, Ibeta */
void calculateClarke(float Ia, float Ib, float Ic, float *Ialpha, float *Ibeta){
	*Ialpha = (1/3.0f)*(2*Ia - Ib - Ic);
	*Ibeta = (1/3.0f)*(SQRT3*Ib - SQRT3*Ic);
	return;
}

/* Park - rotate the (alpha, beta) current into the rotor d/q frame.
 * Input:  Ialpha, Ibeta + sin_t/cos_t
 * Output: Id, Iq */
void calculatePark(float Ialpha, float Ibeta, float theta, float *Iq, float *Id, float sin_t, float cos_t){
	*Id =  Ialpha * cos_t + Ibeta * sin_t;
	*Iq = -Ialpha * sin_t + Ibeta * cos_t;
	return;
}

/* Inverse Park - rotor d/q voltage commands back out to the (alpha, beta) frame.
 * Input:  Ud, Uq + precomputed sin_t/cos_t of theta
 * Output: Ualpha, Ubeta
 * NOTE: the old open-loop/closed-loop split is left commented below; always closed-loop now. */
void calculateInvPark(float *Ualpha, float *Ubeta, float theta, float Uq, float Ud, float sin_t, float cos_t, drive_state_t drive_state){

	*Ualpha = Ud * cos_t - Uq * sin_t;
	*Ubeta  = Ud * sin_t + Uq * cos_t;

//	if(drive_state == STATE_CLOSEDLOOP){
//		*Ualpha = Ud * cos_t - Uq * sin_t;
//		*Ubeta  = Ud * sin_t + Uq * cos_t;
//
//	} else if(drive_state == STATE_INIT){
//		float sin_t_OL, cost_t_OL;
//		cordic_angle = 0;
////		cordic_angle = (int32_t) s_angle_uq31;
//		cordic_calculate(cordic_angle, &sin_t_OL, &cost_t_OL);
//		*Ualpha = Ud * cost_t_OL - Uq * sin_t_OL;
//		*Ubeta  = Ud * sin_t_OL + Uq * cost_t_OL;
//
//	}

	return;
}

/* Sin & cos of an angle straight from the CORDIC hardware.
 * Input:  theta in radians
 * Output: sin_t, cos_t */
void CORDIC_CalculateSinCos(float theta, float *sin_t, float *cos_t){
	float theta_posnegpi;                 // theta folded into -PI..+PI for the CORDIC

	theta_posnegpi = wrap_posneg_pi(theta);
	int32_t theta_q31 = radians_to_q31(theta_posnegpi);
	cordic_calculate(theta_q31, sin_t, cos_t);

}

/* NOTE: not used now */
void CORDIC_Update_Angle(void){
	s_angle_uq31 += s_delta_uq31;
	//CORDIC_Change_Constant_Angle(change_angle);
	s_angle_float = q31_to_radians(s_angle_uq31);
}
