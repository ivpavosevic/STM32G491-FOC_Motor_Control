/*
 * adc_FOC.h
 *
 *  Created on: 15 Jun 2026
 *      Author: ivanp
 */

#ifndef INC_ADC_FOC_H_
#define INC_ADC_FOC_H_

#include "main.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ADC_CAL_SIZE1 100
#define ADC_CAL_SIZE2 1000
#define R_SHUNT 0.001f
#define ADC_MAX_VALUE 4095
#define GAIN 50
#define VREF_MV 2900.0f
#define VREF_V 2.9f
#define T_INIT_WAIT 15000u // 15000 ticks * 40 us = 600ms

#define LOG_SAMPLES   1750        // 2000 × 40 µs = 80 ms at full rate
#define LOG_DECIMATE  1           // log every Nth cycle (raise to widen the window)


#define DELTA_RE_MIN  0.01f    /* ≈ (0.1 A)^2 : below this, current too small to trust */
#define DELTA_MAX     0.10f    /* rad/tick — max physical elec. speed × dt, with margin */


#define OVERCURRENT_LIMIT 4.0f
#define OVERCCURENT_MIN_SAMPLES 2

void log_arm(void);
uint8_t log_done(void);
void log_dump_uart(UART_HandleTypeDef *huart);

/*Use privately the pointer for hadc*/
void ADC_FOC_Init(ADC_HandleTypeDef *hadc, float M1_theta_0, float M2_theta_0);
void DWT_Init(void);
void Setup_Init(foc_user_params *fp, float theta_0);

float convert_ticks_to_s(uint32_t delta_time);
float convert_ticks_to_us(uint32_t delta_time);

float ADC_ConvRawCurrValue(uint16_t raw_v, uint8_t phase, uint8_t motor);
uint8_t ADC1_PopCurrentsValues(adc_conv_t *out, motor_data *Mx);
void ADC_StartCalibration1(void);
void ADC_StartCalibration2(void);

void Get3PhaseV_ABC(float *Ua, float *Ub, float *Uc, uint8_t motor);

#endif /* INC_ADC_FOC_H_ */
