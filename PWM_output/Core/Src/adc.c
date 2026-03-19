/*
 * adc.c
 *
 *
 * Purpose: Init of ADC, raw value conversion and interrupt that calls Clarke & Park transforms
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */

#include "adc.h"
#include "cordic_sin.h"
#include "kalman.h"
#include "control.h"


/* Shared between ISR and main context */
static volatile uint8_t  s_adc_new  = 0;
static volatile uint8_t  s_adc_calibrated  = 0;
static volatile adc_curr_raw_t s_raw;
static foc_i_alfabeta_t s_clarke;
static foc_i_dq_t s_park;

static volatile float current_A;
static volatile float current_B;
static volatile float current_C;

static uint32_t dwtADC1stTime = 0;
static uint32_t dwtADC2ndTime = 0;
static uint32_t dwtTotalTime = 0;

static volatile uint16_t offsetA, offsetB, offsetC;

volatile HallKF kf;
static float dt = 0;
static float theta = 0;
static float theta_measured = 0.0f;

float conv_const = (VREF_V) / (ADC_MAX_VALUE * GAIN * R_SHUNT);

void ADC_Init(ADC_HandleTypeDef *hadc, uint16_t theta_0){
	HAL_ADCEx_InjectedStart_IT(hadc);
	HallKF_Init(&kf, theta_0);
}

/* Used for debugging purposes to identify duration of interrupt */
void DWT_Init(void) {
	// Enable DWT CYCCNT
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable trace and debug
	DWT->CYCCNT = 0;                                // Reset counter
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Enable the cycle counter
}

float convert_ticks_to_us(uint32_t delta_time){
	return delta_time / 100.0f; // 1 tick = 10^-8 sec * 10^6 to be in usec
}

/* Converts raw value of currents Ia, Ib and Ic */
float ADC_ConvRawCurrValue(uint16_t raw_v, uint8_t phase){
	// Convert voltage reading to current with R_shunt = 0.001 Ohm
	int16_t raw_v_offs;
	if(phase == 1){
		raw_v_offs = raw_v - offsetA;
	} else if(phase == 2) {
		raw_v_offs = raw_v - offsetB;
	} else if (phase == 3){
		raw_v_offs = raw_v - offsetC;
	}
	float conv_v = raw_v_offs * conv_const;
	return conv_v;
}

/* Update that new value has been read */
uint8_t ADC1_PopCurrentsValues(adc_curr_raw_t *out)
{
    if (!out) return 0;

    if (!s_adc_new) return 0;

    *out = s_raw;
    s_adc_new = 0;
    return 1;
}

/* Used during intialization when to make an average of all offsets for reading zero A */
void ADC_StartCalibration(ADC_HandleTypeDef *hadc){
	adc_curr_raw_t adc_cal_data;
	uint32_t sumA = 0;
	uint32_t sumB = 0;
	uint32_t sumC = 0;
	uint16_t c = 0;
	HAL_Delay(100);

	while (c < ADC_CAL_SIZE ){
		if(s_adc_new == 1){
			ADC1_PopCurrentsValues(&adc_cal_data);
			sumA += adc_cal_data.ia_raw;
			sumB += adc_cal_data.ib_raw;
			sumC += adc_cal_data.ic_raw;
			c++;
		}
	}

	offsetA = sumA / ADC_CAL_SIZE;
	offsetB = sumB / ADC_CAL_SIZE;
	offsetC = sumC / ADC_CAL_SIZE;

	s_adc_calibrated = 1;
}


/* Interrupt for reading the ADC values and perforimg Clarke and Parke transforms (every 40 us)*/
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{

	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8); // for debugging purpose
	if (hadc->Instance != ADC1)
        return;

    /* Current sensing - pin PA0 */
    s_raw.ia_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);

    /* Current sensing - pin PC0 */
    s_raw.ib_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);

    /* Current sensing - pin PC1 */
    s_raw.ic_raw = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);

    if(s_adc_calibrated == 1){
      // Transform raw to readable data
        current_A = ADC_ConvRawCurrValue(s_raw.ia_raw, 1);
        current_B = ADC_ConvRawCurrValue(s_raw.ib_raw, 2);
        current_C = ADC_ConvRawCurrValue(s_raw.ic_raw, 3);

      // Perform Clarke transform
        calculateClarke(current_A, current_B, current_C, &s_clarke.i_alfa, &s_clarke.i_beta);

      // Perform Kalman filter on angle and speed

      /* Checking duration between interrupts */
        dwtADC2ndTime = DWT->CYCCNT; // Get the cycle value after we had executed our code
        dwtTotalTime = dwtADC2ndTime - dwtADC1stTime; // Calculate how many cycles have passed
    	dwtADC1stTime = DWT->CYCCNT;
    	dt = convert_ticks_to_us(dwtTotalTime);


      //1st step: Kalman prediction - done every ADC interrupt
        KF_Predict(&kf, dt);

      // 2nd step: Kalman update - done every time new Hall data appears
        if(new_Hall_meas_flag == 1){
        	theta_measured = (float) new_Hall_meas_angle;
        	KF_Update(&kf, theta_measured);
        	new_Hall_meas_flag = 0;

        }

      // Perform Park transform
        calculatePark(s_clarke.i_alfa, s_clarke.i_beta, kf.theta, &s_park.i_d, &s_park.i_q);
    }

    s_adc_new  = 1;


    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8); // for debugging purpose

}


float get_Id(void){
	return s_park.i_d;
}

float get_Iq(void){
	return s_park.i_q;
}

float get_Ialfa(void){
	return s_clarke.i_alfa;
}

float get_Ibeta(void){
	return s_clarke.i_beta;
}




