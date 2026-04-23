/*
 * pwm.c
 *
 *
 * Purpose: Direct writing of PWM values to CCR registers (sine wave generation)
 *
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "pwm.h"
#include "cordic_sin.h"
#include "control.h"
#include "uart.h"

static TIM_HandleTypeDef s_htim;

/* Modulation index (0.0 to 1.0) - controls amplitude */
static float s_modulation_index = 0.11f;


/* Set constant PWM duty cycle  */
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

/* Interrupt called every 20 us to overwrite duty cycle */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) { // TO DO check duration of this interrupt
  if (htim->Instance != TIM1){
	  return;
  }
	  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8); // for debugging purpose

	  static float Ua, Ub, Uc;
	  uint32_t ccr1, ccr2, ccr3;
	  static uint32_t counter = 0;

	  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);

	  const float k = 0.5f * s_modulation_index;

	  /* function that fulfills Ua, Ub, Uc */
	  uint32_t t0 = DWT->CYCCNT;
	  Control_Set3PhaseV(&Ua, &Ub, &Uc);
	  uint32_t dt_PWM = DWT->CYCCNT - t0;

	//  Ua = 0.0f;
	//  Ub = 0.0f;
	//  Uc = 0.0f;

	  /* Convert sine (-1 to 1) to duty cycle (0 to 1) */
	  /* 1-Ux is because of PWM mode2 PWM LOW before CNT = CCR and HIGH after */
	  float duty_a = 0.5f - k * Ua;
	  float duty_b = 0.5f - k * Ub;
	  float duty_c = 0.5f - k * Uc;

	  if (duty_a < 0.0f) duty_a = 0.0f;
	  if (duty_a > 1.0f) duty_a = 1.0f;
	  if (duty_b < 0.0f) duty_b = 0.0f;
	  if (duty_b > 1.0f) duty_b = 1.0f;
	  if (duty_c < 0.0f) duty_c = 0.0f;
	  if (duty_c > 1.0f) duty_c = 1.0f;

	  /* Calculate CCR values */
	  ccr1 = (uint32_t)(duty_a * (float)arr);
	  ccr3 = (uint32_t)(duty_b * (float)arr); // direction swapped
	  ccr2 = (uint32_t)(duty_c * (float)arr); // direction swapped

	  /* Set PWM compare values */
	  __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, ccr1);
	  __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, ccr2);
	  __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, ccr3);
	  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8); // for debugging purpose

/* Future implementation */
//  if(htim->Instance == htim2){
//	  	 UART_Logger();
//  }

}

void setCH4duty(TIM_HandleTypeDef *htim, uint32_t pwm4_duty){
	__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_4, pwm4_duty);
	return;
}

