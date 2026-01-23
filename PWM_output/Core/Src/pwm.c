/*
 * pwm.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "pwm.h"
#include "sin_lut.h"
#include "adc.h"

static volatile ccr1_1st = 0;
static volatile ccr1_2nd = 0;

void pwm_set_duty_percent(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t pct)
{
    if (pct > 100) pct = 100;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
    uint32_t ccr = ((arr + 1) * pct) / 100U;
    if (ccr > arr) ccr = arr;
    __HAL_TIM_SET_COMPARE(htim, channel, ccr);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance != TIM1) return;
	float sin_val1, sin_val2, sin_val3;
	uint32_t ccr1, ccr2, ccr3;
	static uint32_t arr;
	static uint16_t idx = 0;
	const float m = 0.5f; // < 1, da nije cijela amplitud
	const float k = 0.5f * m;

	arr = __HAL_TIM_GET_AUTORELOAD(htim);

	sin_val1 = 0.5f + k * sinLUT_Get(idx);
	sin_val2 = 0.5f + k * sinLUT_Get(idx + LUT_OFFSET_120);
	sin_val3 = 0.5f + k * sinLUT_Get(idx + LUT_OFFSET_240);

	ccr1 = (uint32_t) (sin_val1 * (float)arr);
	ccr2 = (uint32_t) (sin_val2 * (float)arr);
	ccr3 = (uint32_t) (sin_val3 * (float)arr);

	__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 1000);
	__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, ccr2);
	__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, ccr3);

	idx+=1;
	if(idx >= SIN_LUT_SIZE) idx = 0;
}

uint16_t PWM_GetCCR1_1st(void){
	return ccr1_1st;
}

uint16_t PWM_GetCCR1_2nd(void){
	return ccr1_2nd;
}


