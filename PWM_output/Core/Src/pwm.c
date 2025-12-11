/*
 * pwm.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */
#include "pwm.h"

void pwm_set_duty_percent(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t pct)
{
    if (pct > 100) pct = 100;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
    uint32_t ccr = ((arr + 1) * pct) / 100U;
    if (ccr > arr) ccr = arr;
    __HAL_TIM_SET_COMPARE(htim, channel, ccr);
}

