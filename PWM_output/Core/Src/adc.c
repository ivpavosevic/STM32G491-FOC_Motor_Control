/*
 * adc.c
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */
#include "adc.h"
/* Shared between ISR and main context */
static volatile uint8_t  s_adc_new  = 0;
static volatile adc_curr_raw_t s_raw;

static volatile uint16_t offsetA, offsetB, offsetC;

const float conv_const = (VREF_MV) / (ADC_MAX_VALUE * GAIN * R_SHUNT);

void ADC_Init(ADC_HandleTypeDef *hadc){
	HAL_ADCEx_InjectedStart_IT(hadc);
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1)
        return;
    //HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8); // for debugging purposes
    /* Current sensing - pin PA0 */
    s_raw.ia_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);

    /* Current sensing - pin PC0 */
    s_raw.ib_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);

    /* Current sensing - pin PC1 */
    s_raw.ic_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
    //HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8); // for debugging purposes

    s_adc_new  = 1;

}

float ADC_ConvRawCurrValue(uint16_t raw_v, uint8_t phase){
	// Convert voltage reading to current with R_shunt = 0.001 Ohm
	int16_t raw_v_offs;
	if(phase == 1){
		raw_v_offs = raw_v - 2048;
	} else if(phase == 2) {
		raw_v_offs = raw_v - 2048;
	} else if (phase == 3){
		raw_v_offs = raw_v - 2048;
	}
	float conv_v = raw_v_offs * conv_const;
	return conv_v;
}



uint8_t ADC1_PopCurrentsValues(adc_curr_raw_t  *out, foc_i_dq_t *out_dq)
{
    if (!out) return 0;

    if (!s_adc_new) return 0;

    *out = s_raw;
    s_adc_new = 0;
    return 1;
}

void ADC_StartCalibration(ADC_HandleTypeDef *hadc){
//	adc_curr_raw_t adc_cal_data;
//	uint32_t sumA = 0;
//	uint32_t sumB = 0;
//	uint32_t sumC = 0;
//	uint16_t c = 0;
//
//	while (c < (ADC_CAL_SIZE + 1) ){
//		if(s_adc_new == 1){
//			ADC1_PopCurrentsValues(&adc_cal_data);
//			sumA += adc_cal_data.ia_raw;
//			sumB += adc_cal_data.ib_raw;
//			sumC += adc_cal_data.ic_raw;
//			c++;
//		}
//	}
//
//	offsetA = sumA / ADC_CAL_SIZE;
//	offsetB = sumB / ADC_CAL_SIZE;
//	offsetC = sumC / ADC_CAL_SIZE;
}
