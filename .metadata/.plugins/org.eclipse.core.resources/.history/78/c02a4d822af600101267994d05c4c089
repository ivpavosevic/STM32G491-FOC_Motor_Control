/*
 * adc.h
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_
#include "main.h"

void ADC_Init(ADC_HandleTypeDef *hadc);
/* Start one injected conversion on ADC1 using a SOFTWARE trigger.
 * Safe to call from ISR (e.g., TIM callback). If ADC is busy, it returns immediately.
 */
void     ADC1_Injected_StartSoft(void);

/* Returns the most recent injected conversion raw sample (12-bit right aligned typically). */
uint16_t ADC1_GetLastSample(void);

/* If a new sample is available, writes it to *out, clears the "new" flag, and returns 1.
 * If no new sample is available, returns 0.
 */
uint8_t  ADC1_TryPopNewSample(uint16_t *out);

/* Returns 1 if an injected conversion is currently running, otherwise 0. */
uint8_t  ADC1_IsBusy(void);


#endif /* INC_ADC_H_ */
