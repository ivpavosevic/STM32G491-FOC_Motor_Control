/*
 * cordic_sin.c
 *
 * CORDIC-based sine/cosine calculation for motor control
 *
 *  Created on: Jan 29, 2026
 *
 *  FIX (Jan 30, 2026):
 *  - Use uint32_t for the phase accumulator and delta to avoid undefined behavior
 *    from signed int32_t overflow when advancing/wrapping angles.
 *  - Keep phase offsets as uint32_t and do additions in unsigned space so 3-phase
 *    offsets (+120°, +240°) remain correct across wrap boundaries.
 */

#include "cordic_sin.h"
#include <stddef.h>
#include <stdint.h>
#include <math.h>

/* ============ Private defines ============ */

/* Q31 format: full scale = 2^31 */
#define Q31_SCALE 2147483648.0f

/* Pi constant */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/*
 * Angle mapping used here:
 *   angle_q31 = (angle_rad / PI) * 2^31
 *   so -PI..+PI maps to int32_t range.
 *
 * 120° = (120/180)*2^31 = 0x55555555
 * 240° = (240/180)*2^31 = 0xAAAAAAAB (which equals -120° in signed)
 *
 * IMPORTANT:
 *  - Store and add offsets in uint32_t so wrap-around is well-defined (mod 2^32).
 *  - Cast to int32_t only when passing the value to the CORDIC peripheral.
 */
#define PHASE_OFFSET_120_UQ31 ((uint32_t)0x55555555u) /* +120° */
#define PHASE_OFFSET_240_UQ31 ((uint32_t)0xAAAAAAABu) /* +240° == -120° (signed) */

/* ============ Private variables ============ */

/* CORDIC handle pointer */
static CORDIC_HandleTypeDef *s_hcordic = NULL;

/* Current angle accumulator in unsigned Q31 domain (wraps mod 2^32) */
static uint32_t s_angle_uq31 = 0;

/* Angle increment per PWM period in unsigned Q31 domain */
static uint32_t s_delta_uq31 = 0;

/* PWM frequency for frequency-to-delta conversion */
static float s_pwm_freq_hz = 50000.0f;

/* Current electrical frequency */
static float s_elec_freq_hz = 1.0f;

static volatile uint32_t counter = 0;

/* ============ Private functions ============ */

int CORDIC_Sin_Init(CORDIC_HandleTypeDef *hcordic_ptr, float pwm_freq_hz) {
  if (hcordic_ptr == NULL || pwm_freq_hz <= 0.0f) {
    return CORDIC_SIN_ERROR;
  }

  s_hcordic = hcordic_ptr;
  s_pwm_freq_hz = pwm_freq_hz;
  s_angle_uq31 = 0;
  s_delta_uq31 = 0;
  s_elec_freq_hz = 0.0f;

  return CORDIC_SIN_OK;
}


/**
 * @brief Convert radians to Q31 format
 *        CORDIC Q31: angle/PI * 2^31
 */
static inline int32_t radians_to_q31(float angle_rad) {
  return (int32_t)((angle_rad / M_PI) * Q31_SCALE);
}
/**
 * @brief Convert Q31 to float (-1.0 to 1.0)
 */
static inline float q31_to_float(int32_t q31_val) {
  return (float)q31_val / Q31_SCALE;
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

/**
 * @brief Calculate sine using CORDIC hardware (blocking/polling)
 * @param angle_q31 Angle in Q31 format
 * @param sin_out Pointer to sine result
 * @param cos_out Pointer to cosine result (can be NULL)
 * @retval CORDIC_SIN_OK on success
 */
static int cordic_calculate(int32_t angle_q31, float *sin_out, float *cos_out) {

  int32_t input = angle_q31;
  int32_t output[2]; /* [0] = sine, [1] = cosine */

  /* Execute CORDIC calculation (polling mode - fastest for single value) */
  if (HAL_CORDIC_Calculate(s_hcordic, &input, output, 1, 10) != HAL_OK) {
    return CORDIC_SIN_ERROR;
  }

  /* Convert results to float */
  if (sin_out != NULL) {
    *sin_out = q31_to_float(output[0]);
  }
  if (cos_out != NULL) {
    *cos_out = q31_to_float(output[1]);
  }

  return CORDIC_SIN_OK;
}

void CORDIC_Change_Constant_Angle(uint8_t angle_flag){
	float angle;
	switch (angle_flag) {
	  case 0:
	  	s_angle_uq31 = 0;
	    break;
	  case 1:
		  angle = degree_to_radians(60);
		  s_angle_uq31 = radians_to_q31(angle);
	    break;
	  case 2:
		  angle = degree_to_radians(120);
		  s_angle_uq31 = radians_to_q31(angle);
	    break;
	  case 3:
		  angle = degree_to_radians(180);
		  s_angle_uq31 = radians_to_q31(angle);
	    break;
	  case 4:
		  angle = degree_to_radians(240);
		  s_angle_uq31 = radians_to_q31(angle);
	    break;
	  case 5:
		  angle = degree_to_radians(300);
		  s_angle_uq31 = radians_to_q31(angle);
	    break;
	  case 6:
		  s_angle_uq31 = 0;
	    break;
	  default:

	}

}


void CORDIC_Sin_SetFrequency(float freq_hz) {
  s_elec_freq_hz = freq_hz;

  /* Calculate angle increment per PWM period:
   * delta_rad = 2*PI * freq_hz / pwm_freq_hz
   * delta_q31 = delta_rad / PI * 2^31
   *           = 2 * freq_hz / pwm_freq_hz * 2^31
   *
   * Store in uint32_t so wrap behavior is defined.
   */
  float delta_normalized = 2.0f * freq_hz / s_pwm_freq_hz;
  s_delta_uq31 = (uint32_t)(delta_normalized * Q31_SCALE);
}

/*
 *
 * Clarke and Park transformations (including inverse)
 *
 */
void calculateInvClarke(float *Ua, float *Ub, float *Uc, float Ualpha, float Ubeta){
	*Ua = Ualpha;
	*Ub = 0.5f*(-Ualpha + sqrtf(3)*Ubeta);
	*Uc = 0.5f*(-Ualpha - sqrtf(3)*Ubeta);
	return;
}

void calculateClarke(float Ia, float Ib, float Ic, float *Ialpha, float *Ibeta){
	*Ialpha = (1/3.0f)*(2*Ia - Ib - Ic);
	*Ibeta = (1/3.0f)*(sqrt(3)*Ib - sqrt(3)*Ic);
	return;
}

void calculatePark(float Ialpha, float Ibeta, float *Iq, float *Id){
	float sin_t, cos_t;

	int result = cordic_calculate((int32_t)s_angle_uq31, &sin_t, &cos_t);

	*Id =  Ialpha * cos_t + Ibeta * sin_t;
	*Iq = -Ialpha * sin_t + Ibeta * cos_t;
	return;
}

void calculateInvPark(float *Ualpha, float *Ubeta, float Uq, float Ud){
	float sin_t = 0, cos_t = 0;

	int result = cordic_calculate((int32_t)s_angle_uq31, &sin_t, &cos_t);

	*Ualpha = Ud * cos_t - Uq * sin_t;
	*Ubeta  = Ud * sin_t + Uq * cos_t;

	//s_angle_uq31 = 0;
	s_angle_uq31 += s_delta_uq31;
	return;
}



int32_t CORDIC_Get_Angle(void){
	return q31_angle_to_deg(s_angle_uq31);
}


int CORDIC_Sin_Get3Phase(float *sin_a, float *sin_b, float *sin_c) {
  int result;

  /* Phase A - current angle */
  result = cordic_calculate((int32_t)s_angle_uq31, sin_a, NULL);
  if (result != CORDIC_SIN_OK) return result;

  /* Phase B - 120° offset */
  result = cordic_calculate((int32_t)(s_angle_uq31 + PHASE_OFFSET_120_UQ31), sin_b, NULL);
  if (result != CORDIC_SIN_OK) return result;

  /* Phase C - 240° offset */
  result = cordic_calculate((int32_t)(s_angle_uq31 + PHASE_OFFSET_240_UQ31), sin_c, NULL);
  if (result != CORDIC_SIN_OK) return result;

  s_angle_uq31 += s_delta_uq31;
  //CORDIC_Change_Constant_Angle(0); // number in brackets is angle * 60
  //s_angle_uq31 = 0;

  return CORDIC_SIN_OK;
}





