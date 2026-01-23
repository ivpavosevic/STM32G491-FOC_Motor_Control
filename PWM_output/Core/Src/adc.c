/*
 * adc.c
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */
#include "adc.h"

static ADC_HandleTypeDef *s_hadc;

/* Shared between ISR and main context */
static volatile uint8_t  s_adc_new  = 0;
static volatile uint16_t s_adc_last = 0;

void ADC_Init(ADC_HandleTypeDef *hadc){

	HAL_ADCEx_InjectedStart_IT(hadc);
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8);
    if (hadc->Instance != ADC1)
        return;

    /* Read injected rank 1 result */
    s_adc_last = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);

    s_adc_new  = 1;

}

uint16_t ADC1_GetLastSample(void)
{
    return s_adc_last;
}

uint8_t ADC1_TryPopNewSample(uint16_t *out)
{
    if (!out)
        return 0;

    if (!s_adc_new)
        return 0;

    /* Read then clear the flag */
    *out = s_adc_last;
    s_adc_new = 0;
    return 1;
}
