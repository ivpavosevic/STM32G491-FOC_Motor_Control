/*
 * pwm.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "pwm.h"
#include "cordic_sin.h"

/* ============ Private variables ============ */

static volatile uint16_t ccr1_1st = 0;
static volatile uint16_t ccr1_2nd = 0;

/* Modulation index (0.0 to 1.0) - controls amplitude */
static float s_modulation_index = 1.0f;

/* ============ Public functions ============ */

/* Set constant PWM duty cycle (for testing) */
void pwm_set_duty_percent(TIM_HandleTypeDef *htim, uint32_t channel,
                          uint32_t pct) {
  if (pct > 100)
    pct = 100;
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
  uint32_t ccr = ((arr + 1) * pct) / 100U;
  if (ccr > arr)
    ccr = arr;
  __HAL_TIM_SET_COMPARE(htim, channel, ccr);
}

/**
 * @brief Set motor electrical frequency
 * @param freq_hz Electrical frequency in Hz
 */
void PWM_SetMotorFrequency(float freq_hz) { CORDIC_Sin_SetFrequency(freq_hz); }

/**
 * @brief Get current motor electrical frequency
 * @retval Frequency in Hz
 */
float PWM_GetMotorFrequency(void) { return CORDIC_Sin_GetFrequency(); }

/**
 * @brief Set modulation index (amplitude)
 * @param m Modulation index 0.0 to 1.0
 */
void PWM_SetModulationIndex(float m) {
  if (m < 0.0f)
    m = 0.0f;
  if (m > 1.0f)
    m = 1.0f;
  s_modulation_index = m;
}

/**
 * @brief Get current modulation index
 * @retval Modulation index
 */
float PWM_GetModulationIndex(void) { return s_modulation_index; }

/* TIM1 Period Elapsed Callback - generates 3-phase PWM using CORDIC */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance != TIM1)
    return;

  float sin_a, sin_b, sin_c;
  uint32_t ccr1, ccr2, ccr3;
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);

  const float k = 0.5f * s_modulation_index;

  /* Get 3-phase sine values using CORDIC */
  if (CORDIC_Sin_Get3Phase(&sin_a, &sin_b, &sin_c) == CORDIC_SIN_OK) {
    /* Convert sine (-1 to 1) to duty cycle (0 to 1) */
    float duty_a = 0.5f + k * sin_a;
    float duty_b = 0.5f + k * sin_b;
    float duty_c = 0.5f + k * sin_c;

    /* Calculate CCR values */
    ccr1 = (uint32_t)(duty_a * (float)arr);
    ccr2 = (uint32_t)(duty_b * (float)arr);
    ccr3 = (uint32_t)(duty_c * (float)arr);

    /* Set PWM compare values */
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, ccr1);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2,
                          ccr3); /* Note: swapped as in original */
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, ccr2);
  }
}

uint16_t PWM_GetCCR1_1st(void) { return ccr1_1st; }

uint16_t PWM_GetCCR1_2nd(void) { return ccr1_2nd; }
