/*
 * pwm.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "pwm.h"
#include "cordic_sin.h"

/* ============ Private variables ============ */
/* Modulation index (0.0 to 1.0) - controls amplitude */
static float s_modulation_index = 0.1f;

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

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance != TIM1)
    return;

  float Ua, Ub, Uc;
  uint32_t ccr1, ccr2, ccr3;
  static uint32_t counter = 0;

  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);

  const float k = 0.5f * s_modulation_index;

  //function that fulfills Ua, Ub, Uc

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
  __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, ccr2);
  __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, ccr3);

}

/********************************************
 *
 *
 *
 * LEGACY CODE
 *
 *
 ********************************************/
/* TIM1 Period Elapsed Callback - generates 3-phase PWM using CORDIC */
//void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
//  if (htim->Instance != TIM1)
//    return;
//
//  float sin_a, sin_b, sin_c;
//  uint32_t ccr1, ccr2, ccr3;
//  static uint32_t counter = 0;
//
//  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
//
//  const float k = 0.5f * s_modulation_index;
//
//  /* Get 3-phase sine values using CORDIC */
//  if (CORDIC_Sin_Get3Phase(&sin_a, &sin_b, &sin_c) == CORDIC_SIN_OK) {
//    /* Convert sine (-1 to 1) to duty cycle (0 to 1) */
//    float duty_a = 0.5f + k * sin_a;
//    float duty_b = 0.5f + k * sin_b;
//    float duty_c = 0.5f + k * sin_c;
//
//    /* Calculate CCR values */
//    ccr1 = (uint32_t)(duty_a * (float)arr);
//    ccr2 = (uint32_t)(duty_b * (float)arr);
//    ccr3 = (uint32_t)(duty_c * (float)arr);
//
//    /* Set PWM compare values */
//    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, ccr1);
//    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, ccr2);
//    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, ccr3);
//  }
//}

