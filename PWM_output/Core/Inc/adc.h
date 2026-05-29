/*
 * adc.h
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_
#include "main.h"
#include <math.h>

#define ADC_CAL_SIZE 100
#define R_SHUNT 0.001f
#define ADC_MAX_VALUE 4095
#define GAIN 50
#define VREF_MV 2900.0f
#define VREF_V 2.9f

typedef struct {
    uint16_t ia_raw;
    uint16_t ib_raw;
    uint16_t ic_raw;
} adc_curr_raw_t;

typedef enum{
	STATE_OPENLOOP,
	STATE_CLOSEDLOOP
} drive_state_t;

typedef struct{
	float Ud;
	float Uq;
	float rotor_freq;
	float Id_ref;
	float Iq_ref;
	float omega_ref;
	float theta;
	drive_state_t drive_state;
} FOC_user_params;

typedef struct {
    float Ua;
    float Ub;
    float Uc;
    float i_alfa;
    float i_beta;
} foc_params_clarke;

typedef struct {
    float Ualfa;
    float Ubeta;
    float i_d;
    float i_q;
	float sin_t;
	float cos_t;
} foc_params_park;

/*Use privately the pointer for hadc*/
void ADC_Init(ADC_HandleTypeDef *hadc, float theta_0);

void DWT_Init(void);

/* If a new sample is available, writes it to *out, clears the "new" flag, and returns 1.
 * If no new sample is available, returns 0.
 */
uint8_t ADC1_PopCurrentsValues(adc_curr_raw_t *out);

float ADC_ConvRawCurrValue(uint16_t raw_v, uint8_t phase);

void ADC_StartCalibration(ADC_HandleTypeDef *hadc);

float convert_ticks_to_s(uint32_t delta_time);

float convert_ticks_to_us(uint32_t delta_time);

void Get3PhaseV_ABC(float *Ua, float *Ub, float *Uc);

float get_Id(void);
float get_Iq(void);
float get_Ialfa(void);
float get_Ibeta(void);


#endif /* INC_ADC_H_ */
