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

#define K_STANDSTILL 6.0f * 1e3
#define T_INIT_WAIT 15000u // 15000 ticks * 40 us = 600ms
#define IQ_SUM_ERR_INIT 0.1f

#define LOG_SAMPLES   1750        // 2000 × 40 µs = 80 ms at full rate
#define LOG_DECIMATE  100           // log every Nth cycle (raise to widen the window)

#define OVERCURRENT_LIMIT_A   4.0f
#define OVERCCURENT_MIN_SAMPLES 3
extern volatile uint8_t overcurrent_fault;

typedef struct {
    float theta_est;     // hkf.theta
    float omega;         // hkf.omega
    float omega_e;    // hkf.last_innovation
    float i_q;           // s_park.i_q
    float d_omega;           // s_park.i_d
    float u_q;           // fp.Uq
    float iq_ref;           // fp.Ud
    float hall_angle;    // new_Hall_meas_angle
} log_sample_t;          // 8 floats = 32 B/sample


typedef struct {
    uint16_t ia_raw;
    uint16_t ib_raw;
    uint16_t ic_raw;
} adc_curr_raw_t;

typedef enum {
	STATE_INIT,
	STATE_CLOSEDLOOP,
	STATE_CALIBRATE
} drive_state_t;

typedef enum {
	SYSTEM_ON,
	SYSTEM_OFF
} system_state_t;

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



void log_arm(void);
uint8_t log_done(void);
void log_dump_uart(UART_HandleTypeDef *huart);

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
