/*
 * pwm.c
 *
 *
 *
 * Purpose: Direct writing of PWM values to CCR registers (sine wave generation)
 *
 *
 *  Created on: 15 Jun 2026
 *      Author: ivanp
 */
#include "pwm.h"

/* ============ Variables ============ */

/* How much of the DC bus we actually use (0.0..1.0) - amplitude of the output sine. */
volatile float s_modulation_index = 0.1f;


/* Voltage from the FOC math (inverse Clarke output). */
volatile float M1_Ua = 0.0f;   // motor 1, phase A
volatile float M1_Ub = 0.0f;   // motor 1, phase B
volatile float M1_Uc = 0.0f;   // motor 1, phase C
volatile float M2_Ua = 0.0f;   // motor 2, phase A
volatile float M2_Ub = 0.0f;   // motor 2, phase B
volatile float M2_Uc = 0.0f;   // motor 2, phase C

/* Duty cycle per phase, re-centred on 50% and scaled by the modulation index later in the ISR function. */
volatile float M1_duty_a = 0.0f;
volatile float M1_duty_b = 0.0f;
volatile float M1_duty_c = 0.0f;
volatile float M2_duty_a = 0.0f;
volatile float M2_duty_b = 0.0f;
volatile float M2_duty_c = 0.0f;

/* The final numbers stored in CCR registers from which PWM can be generated */
volatile uint32_t M1_ccr1 = 0;
volatile uint32_t M1_ccr2 = 0;
volatile uint32_t M1_ccr3 = 0;
volatile uint32_t M2_ccr1 = 0;
volatile uint32_t M2_ccr2 = 0;
volatile uint32_t M2_ccr3 = 0;


/* Timer handles stashed at init so the rest of the file can reach them. */
static volatile TIM_HandleTypeDef *s_htim1 = NULL;   // TIM1 -> motor 1
static volatile TIM_HandleTypeDef *s_htim8 = NULL;   // TIM8 -> motor 2

/* ============ Private functions ============ */

/* Stores pointers of timers into static variables of this file */
void PWM_Init(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim8){
	s_htim1 = htim1;   // motor 1
	s_htim8 = htim8;   // motor 2
}


/* Set a fixed duty cycle on one channel, given as a whole percent (0..100). Made for bring up and initial testing procedures. */
void pwm_set_duty_percent(TIM_HandleTypeDef *htim, uint32_t channel,
                          uint32_t pct) {
  if (pct > 100)
    pct = 100;                                    // don't allow more than 100%
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);  // timer period (max count)
  uint32_t ccr = ((arr + 1) * pct) / 100U;        // percent -> compare value
  if (ccr > arr)
    ccr = arr;                                    // never exceed the period
  __HAL_TIM_SET_COMPARE(htim, channel, ccr);      // hand it to the hardware
}

/* Trigger for the ADC injected sampling. Point in time when ADC can start sampling.
 * NOTE: Needs to happen when all 3 PWMs are low and when inverted PWMs are high output. */
void setCH4duty(TIM_HandleTypeDef *htim, uint32_t pwm4_duty){
	__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_4, pwm4_duty);
	return;
}

/* ============ Interrupt functions ============ */

/* Fires every 20 us on the TIM1 update event.
 * Input: Voltage levels from FOC algorithm
 * Output: New duty cycle stored in all 3-phases of PWM generation */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) { // TO DO check duration of this interrupt
	if (htim->Instance != TIM1){
	  return;                      // only TIM1's update drives this; ignore any other timer
	}

	/* Pull the latest phase-voltage references from the FOC math, one per motor */
	Get3PhaseV_ABC(&M1_Ua, &M1_Ub, &M1_Uc, 1u);
	Get3PhaseV_ABC(&M2_Ua, &M2_Ub, &M2_Uc, 2u);
//	CORDIC_Update_Angle();
	/* Capture the arr register of TIM1 (the period, in ticks) */
	uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);

	/* Constant offset at V/2 */
	/* k is the per-phase swing around the 50% midpoint (half the modulation index) */
	const float k = 0.5f * s_modulation_index;

	/* Convert sine (-1 to 1) to duty cycle (0 to 1), centred on 50% */
	/* 1-Ux is because of PWM mode2: PWM LOW before CNT==CCR and HIGH after CNT==CCR */
	/* (so a bigger positive voltage means a smaller duty number - therefore the minus) */
	M1_duty_a = 0.5f - k * M1_Ua;
	M1_duty_b = 0.5f - k * M1_Ub;
	M1_duty_c = 0.5f - k * M1_Uc;

	M2_duty_a = 0.5f - k * M2_Ua;
	M2_duty_b = 0.5f - k * M2_Ub;
	M2_duty_c = 0.5f - k * M2_Uc;

	/* Clamp duty for safety - a bad reference should never push us outside 0..100% */
	if (M1_duty_a < 0.0f) M1_duty_a = 0.0f;
	if (M1_duty_a > 1.0f) M1_duty_a = 1.0f;
	if (M1_duty_b < 0.0f) M1_duty_b = 0.0f;
	if (M1_duty_b > 1.0f) M1_duty_b = 1.0f;
	if (M1_duty_c < 0.0f) M1_duty_c = 0.0f;
	if (M1_duty_c > 1.0f) M1_duty_c = 1.0f;

	if (M2_duty_a < 0.0f) M2_duty_a = 0.0f;
	if (M2_duty_a > 1.0f) M2_duty_a = 1.0f;
	if (M2_duty_b < 0.0f) M2_duty_b = 0.0f;
	if (M2_duty_b > 1.0f) M2_duty_b = 1.0f;
	if (M2_duty_c < 0.0f) M2_duty_c = 0.0f;
	if (M2_duty_c > 1.0f) M2_duty_c = 1.0f;

	/* Turn each duty fraction into an actual compare value (duty * period) */
	M1_ccr1 = (uint32_t)(M1_duty_a * (float)arr);
	M1_ccr2 = (uint32_t)(M1_duty_b * (float)arr);
	M1_ccr3 = (uint32_t)(M1_duty_c * (float)arr);

	M2_ccr1 = (uint32_t)(M2_duty_a * (float)arr);
	M2_ccr2 = (uint32_t)(M2_duty_b * (float)arr);
	M2_ccr3 = (uint32_t)(M2_duty_c * (float)arr);

	/* Write the new compares to the hardware - TIM1 = motor 1, TIM8 = motor 2. */
	__HAL_TIM_SET_COMPARE(s_htim1, TIM_CHANNEL_1, M1_ccr1);
	__HAL_TIM_SET_COMPARE(s_htim1, TIM_CHANNEL_2, M1_ccr2);
	__HAL_TIM_SET_COMPARE(s_htim1, TIM_CHANNEL_3, M1_ccr3);

	__HAL_TIM_SET_COMPARE(s_htim8, TIM_CHANNEL_1, M2_ccr1);
	__HAL_TIM_SET_COMPARE(s_htim8, TIM_CHANNEL_2, M2_ccr2);
	__HAL_TIM_SET_COMPARE(s_htim8, TIM_CHANNEL_3, M2_ccr3);



/* Future implementation */
//  if(htim->Instance == htim2){
//	  	 UART_Logger();
//  }

}



