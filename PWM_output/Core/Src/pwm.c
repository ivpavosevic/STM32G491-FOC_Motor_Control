/*
 * pwm.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "pwm.h"
#include "sin_lut.h"

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

	static uint16_t idx = 0;
	static float m = 1.0f; // < 1, da nije cijela amplituda

	uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);

	float sin_val1 = 0.5f + 0.5f * m * sinLUT_Get(idx);
	float sin_val2 = 0.5f + 0.5f * m * sinLUT_Get(idx + LUT_OFFSET_120);
	float sin_val3 = 0.5f + 0.5f * m * sinLUT_Get(idx + LUT_OFFSET_240);

	uint32_t ccr1 = (uint32_t) (sin_val1 * (float)arr);
	uint32_t ccr2 = (uint32_t) (sin_val2 * (float)arr);
	uint32_t ccr3 = (uint32_t) (sin_val3 * (float)arr);

//	uint32_t ccr1 = 1000;
//	uint32_t ccr2 = 2125;
//	uint32_t ccr3 = 3500;

	__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, ccr1);
	__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, ccr2);
	__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, ccr3);

	idx++;
	if(idx >= SIN_LUT_SIZE) idx = 0;
}
