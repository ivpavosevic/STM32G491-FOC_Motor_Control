/*
 * adc.h
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_
#include "main.h"
#define ADC_TO_MV(x) ((x) * 3300UL / 4095UL)


/*Use privately the pointer for hadc*/
void ADC_Init(ADC_HandleTypeDef *hadc);


/* Returns the most recent injected conversion raw sample (12-bit right aligned typically). */
uint16_t ADC1_GetLastSample(void);

/* If a new sample is available, writes it to *out, clears the "new" flag, and returns 1.
 * If no new sample is available, returns 0.
 */
uint8_t  ADC1_TryPopNewSample(uint16_t *out);



#endif /* INC_ADC_H_ */
