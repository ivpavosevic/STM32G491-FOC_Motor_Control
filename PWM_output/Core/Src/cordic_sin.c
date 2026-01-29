/*
 * cordic_sin.c
 *
 * CORDIC-based sine/cosine calculation for motor control
 *
 *  Created on: Jan 29, 2026
 */

#include "cordic_sin.h"
#include <stddef.h>

/* ============ Private defines ============ */

/* Q31 format: full scale = 2^31 */
#define Q31_SCALE 2147483648.0f

/* Pi constant */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Phase offsets in Q31 format for 3-phase motor */
/* 120° = 2*PI/3 rad = 1/3 of full circle in Q31 */
#define PHASE_OFFSET_120_Q31 ((int32_t)0x2AAAAAAB) /* ~715827883 */
#define PHASE_OFFSET_240_Q31 ((int32_t)0x55555555) /* ~1431655765 */

/* ============ Private variables ============ */

/* CORDIC handle pointer */
static CORDIC_HandleTypeDef *s_hcordic = NULL;

/* Current angle in Q31 format (-PI to PI maps to full int32_t range) */
static int32_t s_angle_q31 = 0;

/* Angle increment per PWM period in Q31 format */
static int32_t s_delta_q31 = 0;

/* PWM frequency for frequency-to-delta conversion */
static float s_pwm_freq_hz = 25000.0f;

/* Current electrical frequency */
static float s_elec_freq_hz = 0.0f;

/* ============ Private functions ============ */

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

/**
 * @brief Calculate sine using CORDIC hardware (blocking/polling)
 * @param angle_q31 Angle in Q31 format
 * @param sin_out Pointer to sine result
 * @param cos_out Pointer to cosine result (can be NULL)
 * @retval CORDIC_SIN_OK on success
 */
static int cordic_calculate(int32_t angle_q31, float *sin_out, float *cos_out) {
  if (s_hcordic == NULL) {
    return CORDIC_SIN_ERROR;
  }

  int32_t input = angle_q31;
  int32_t output[2]; /* [0] = sine, [1] = cosine */

  /* Configure CORDIC for sine calculation */
  CORDIC_ConfigTypeDef config = {0};
  config.Function = CORDIC_FUNCTION_SINE;
  config.Scale = CORDIC_SCALE_0;
  config.InSize = CORDIC_INSIZE_32BITS;
  config.OutSize = CORDIC_OUTSIZE_32BITS;
  config.NbWrite = CORDIC_NBWRITE_1;
  config.NbRead = (cos_out != NULL) ? CORDIC_NBREAD_2 : CORDIC_NBREAD_1;
  config.Precision = CORDIC_PRECISION_6CYCLES; /* 6 cycles = good precision */

  if (HAL_CORDIC_Configure(s_hcordic, &config) != HAL_OK) {
    return CORDIC_SIN_ERROR;
  }

  /* Execute CORDIC calculation (polling mode - fastest for single value) */
  uint32_t nb_out = (cos_out != NULL) ? 2 : 1;
  if (HAL_CORDIC_Calculate(s_hcordic, &input, output, 1, HAL_MAX_DELAY) !=
      HAL_OK) {
    return CORDIC_SIN_ERROR;
  }

  /* Convert results to float */
  if (sin_out != NULL) {
    *sin_out = q31_to_float(output[0]);
  }
  if (cos_out != NULL && nb_out == 2) {
    *cos_out = q31_to_float(output[1]);
  }

  return CORDIC_SIN_OK;
}

/* ============ Public API Implementation ============ */

int CORDIC_Sin_Init(CORDIC_HandleTypeDef *hcordic_ptr, float pwm_freq_hz) {
  if (hcordic_ptr == NULL || pwm_freq_hz <= 0.0f) {
    return CORDIC_SIN_ERROR;
  }

  s_hcordic = hcordic_ptr;
  s_pwm_freq_hz = pwm_freq_hz;
  s_angle_q31 = 0;
  s_delta_q31 = 0;
  s_elec_freq_hz = 0.0f;

  return CORDIC_SIN_OK;
}

void CORDIC_Sin_SetFrequency(float freq_hz) {
  s_elec_freq_hz = freq_hz;

  /* Calculate angle increment per PWM period:
   * delta_rad = 2*PI * freq_hz / pwm_freq_hz
   * delta_q31 = delta_rad / PI * 2^31
   *           = 2 * freq_hz / pwm_freq_hz * 2^31
   */
  float delta_normalized = 2.0f * freq_hz / s_pwm_freq_hz;
  s_delta_q31 = (int32_t)(delta_normalized * Q31_SCALE);
}

float CORDIC_Sin_GetFrequency(void) { return s_elec_freq_hz; }

void CORDIC_Sin_ResetAngle(void) { s_angle_q31 = 0; }

int CORDIC_Sin_Get3Phase(float *sin_a, float *sin_b, float *sin_c) {
  int result;

  /* Phase A - current angle */
  result = cordic_calculate(s_angle_q31, sin_a, NULL);
  if (result != CORDIC_SIN_OK)
    return result;

  /* Phase B - 120° offset */
  result = cordic_calculate(s_angle_q31 + PHASE_OFFSET_120_Q31, sin_b, NULL);
  if (result != CORDIC_SIN_OK)
    return result;

  /* Phase C - 240° offset */
  result = cordic_calculate(s_angle_q31 + PHASE_OFFSET_240_Q31, sin_c, NULL);
  if (result != CORDIC_SIN_OK)
    return result;

  /* Auto-advance angle */
  s_angle_q31 += s_delta_q31;

  return CORDIC_SIN_OK;
}

int CORDIC_Sin_Get(float *sin_out, float *cos_out) {
  int result = cordic_calculate(s_angle_q31, sin_out, cos_out);

  if (result == CORDIC_SIN_OK) {
    /* Auto-advance angle */
    s_angle_q31 += s_delta_q31;
  }

  return result;
}

int CORDIC_Sin_GetWithOffset(float phase_offset_deg, float *sin_out) {
  float offset_rad = phase_offset_deg * M_PI / 180.0f;
  int32_t offset_q31 = radians_to_q31(offset_rad);

  return cordic_calculate(s_angle_q31 + offset_q31, sin_out, NULL);
}

void CORDIC_Sin_Advance(void) { s_angle_q31 += s_delta_q31; }
