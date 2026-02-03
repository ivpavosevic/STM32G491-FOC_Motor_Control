/*
 * adc.h
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_
#include "main.h"

#define ADC_CAL_SIZE 256
#define R_SHUNT 0.001f
#define ADC_MAX_VALUE 4095
#define GAIN 50
#define VREF 3.3f

#define ADC_TO_MV(x) ((x) * 3300UL / 4095UL)

typedef struct {
    uint16_t ia_raw;
    uint16_t ib_raw;
    uint16_t ic_raw;
} adc_curr_raw_t;


/*Use privately the pointer for hadc*/
void ADC_Init(ADC_HandleTypeDef *hadc);

/* If a new sample is available, writes it to *out, clears the "new" flag, and returns 1.
 * If no new sample is available, returns 0.
 */
uint8_t ADC1_PopCurrentsValues(adc_curr_raw_t *out);

float ADC_ConvRawCurrValue(uint16_t raw_v, uint8_t phase);

#endif /* INC_ADC_H_ */
