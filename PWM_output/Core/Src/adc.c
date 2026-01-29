/*
 * adc.c
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */
#include "adc.h"
volatile uint32_t adc_isr_cnt = 0;

/* Shared between ISR and main context */
static volatile uint8_t  s_adc_new  = 0;
static volatile adc_curr_raw_t s_raw;

static volatile uint16_t I_a = 0;
static volatile uint16_t I_b = 0;
static volatile uint16_t I_c = 0;


void ADC_Init(ADC_HandleTypeDef *hadc){

	HAL_ADCEx_InjectedStart_IT(hadc);
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1)
        return;
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8); // for debugging purposes
    /* Current sensing - pin PA0 */
    s_raw.ia_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);

    /* Current sensing - pin PC0 */
    s_raw.ib_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);

    /* Current sensing - pin PC1 */
    s_raw.ic_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8); // for debugging purposes

    s_adc_new  = 1;

}

float ADC_ConvRawCurrValue(uint16_t raw_v){
	// Convert voltage reading to current with R_shunt = 0.001 Ohm
	uint32_t conv_v = ADC_TO_MV(raw_v);
	return (float) conv_v / R_shunt;
}


uint8_t ADC1_PopCurrentsValues(adc_curr_raw_t  *out)
{
    if (!out) return 0;

    if (!s_adc_new) return 0;

    *out = s_raw;
    s_adc_new = 0;
    return 1;
}
