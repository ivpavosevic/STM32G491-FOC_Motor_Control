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
#include "ang_est_vectors.h"


/* Variables used for current reading from ADC */
static volatile uint8_t  s_adc_new  = 0;
static volatile uint8_t  s_adc_calibrated  = 0;
static volatile adc_curr_raw_t s_raw;
static volatile foc_params_clarke s_clarke;
static volatile foc_params_park s_park;

volatile float current_A;
volatile float current_B;
volatile float current_C;
volatile float sum_ABC = 0.0f;

volatile PI_reg_t pi_id;
volatile PI_reg_t pi_iq;
volatile PI_reg_t pi_omega;
volatile FOC_user_params fp;
volatile float id_err = 0.0f;
volatile float iq_err = 0.0f;
volatile float omega_err = 0.0f;

static float theta_adc = 0.0f;
static float theta_raw = 0.0f;
static float theta_measured = 0.0f;

static uint32_t dwtADC1stTime = 0;
static uint32_t dwtADC2ndTime = 0;
static uint32_t dwtTotalTime = 0;

static volatile uint16_t offsetA, offsetB, offsetC;

static HallKF_data hkf;
static CurrVectorKF_data cvkf;
volatile float d_omega;
static float dt = 0.0f;
static volatile uint32_t counter_cycle = 0;

static int32_t overrun_count = 0;

static int32_t adc_isr_count = 0;
volatile float adc_iq_sum = 0.0f;
volatile float adc_id_sum = 0.0f;

volatile uint32_t steps_since_hall = 0;



float conv_const = (VREF_V) / (ADC_MAX_VALUE * GAIN * R_SHUNT);

void ADC_Init(ADC_HandleTypeDef *hadc, float theta_0){
	HAL_ADCEx_InjectedStart_IT(hadc);
	HallKF_Init(&hkf, theta_0);
	CurrVectorKF_Init(&cvkf, theta_0);
	d_omega = 0.0f;
	// Setup of PI parameters for Id and Iq
	PI_Init_d(&pi_id);
	PI_Init_q(&pi_iq);
	PI_Init_omega(&pi_omega);
	Setup_Init(&fp, theta_0);
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

float convert_ticks_to_s(uint32_t delta_time){
	return delta_time / 1e8f; // 1 tick = 10^-8
}


static inline float atan_approx_core(float x) {
    float x2 = x * x;
    return x * (0.99997726f + x2 * (-0.33262347f + x2 * (0.19354346f +
           x2 * (-0.11643287f + x2 * (0.05265332f + x2 * -0.01172120f)))));
}

static inline float atan2_fast(float y, float x) {
    int swap = __builtin_fabsf(x) < __builtin_fabsf(y);
    float input = (swap ? x : y) / (swap ? y : x);
    float result = atan_approx_core(input);
    if (swap) result = (input >= 0.0f ? M_PI_2 : -M_PI_2) - result;
    if (x < 0.0f) result = (y >= 0.0f ? M_PI : -M_PI) + result;
    return result;
}

/* Converts raw value of currents Ia, Ib and Ic */
float ADC_ConvRawCurrValue(uint16_t raw_v, uint8_t phase){
	// Convert voltage reading to current with R_shunt = 0.001 Ohm
	int16_t raw_v_offs = 0;
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

/*
 *
 * Updating voltage levels on three-phase gates
 *
 * */
void Get3PhaseV_ABC(float *Ua, float *Ub, float *Uc) {
	*Ua = s_clarke.Ua;
	*Ub = s_clarke.Ub;
	*Uc = s_clarke.Uc;
}

/* Interrupt for reading the ADC values and perforimg Clarke and Parke transforms (every 40 us)*/
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{

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
	current_A = -ADC_ConvRawCurrValue(s_raw.ia_raw, 1);
	current_B = -ADC_ConvRawCurrValue(s_raw.ib_raw, 2);
	current_C = -ADC_ConvRawCurrValue(s_raw.ic_raw, 3);

	//        sum_ABC = current_A + current_B + current_C;

	// Perform Clarke transform
	calculateClarke(current_A, current_B, current_C, &s_clarke.i_alfa, &s_clarke.i_beta);

	/* Checking duration between interrupts */
	dwtADC2ndTime = DWT->CYCCNT; // Get the cycle value after we had executed our code
	dwtTotalTime = dwtADC2ndTime - dwtADC1stTime; // Calculate how many cycles have passed
	dwtADC1stTime = DWT->CYCCNT;
	dt = convert_ticks_to_s(dwtTotalTime);
	d_omega = convert_ticks_to_us(dwtTotalTime);

	theta_raw = atan2_fast(s_clarke.i_beta, s_clarke.i_alfa);
	theta_adc = (theta_raw < 0.0f) ? theta_raw + 2.0f * M_PI : theta_raw;
	// wrap to keep within [0, 2*PI]
	if (theta_adc >  2*M_PI) theta_adc -= 2.0f * M_PI;
	if (theta_adc <  0.0f) theta_adc += 2.0f * M_PI;


	if(system_on_off == 1){
		if (fp.drive_state == STATE_CLOSEDLOOP){
			//1st step: Kalman prediction - done every ADC interrupt
			HKF_Predict(&hkf, dt);
			CVKF_Predict(&cvkf, hkf.omega, dt);

			// 2nd step: Kalman update - done every time new Hall data appears
			if(new_Hall_meas_flag == 1){
				theta_measured = (float) new_Hall_meas_angle;
				HKF_Update(&hkf, theta_measured);
				CVKF_Update(&cvkf, theta_adc);
				new_Hall_meas_flag = 0;
			}
			fp.theta = hkf.theta;
		} else if(fp.drive_state == STATE_OPENLOOP){
			counter_cycle++;
			if(counter_cycle >= 40000){
				pi_iq.sum_err = fp.Uq / (2.0f*pi_iq.Ki);
				fp.drive_state = STATE_CLOSEDLOOP;
				counter_cycle = 0;// threshold in rad/s
			}
		}


	}

	//		adc_isr_count++;
	//		adc_id_sum += s_park.i_d;
	//		adc_iq_sum += s_park.i_q;



	CORDIC_CalculateSinCos(fp.theta, &s_park.sin_t, &s_park.cos_t);

	calculatePark(s_clarke.i_alfa, s_clarke.i_beta, fp.theta, &s_park.i_q, &s_park.i_d, s_park.sin_t, s_park.cos_t);

	// PI regulation
	if(fp.drive_state == STATE_CLOSEDLOOP){
		id_err = fp.Id_ref - s_park.i_d;
		iq_err = fp.Iq_ref - s_park.i_q;
		fp.Ud = Control_PI_reg(&pi_id, id_err, dt);
		fp.Uq = Control_PI_reg(&pi_iq, iq_err, dt);
		if(counter_cycle++ >= 10){
			omega_err = fp.omega_ref - hkf.omega;
//			fp.Iq_ref = Control_PI_reg(&pi_omega, omega_err, dt * 10);
			//fp.Iq_ref = 0.5f;
			counter_cycle = 0;
		}

	}

	// Inverse Park
	calculateInvPark(&s_park.Ualfa, &s_park.Ubeta, fp.theta, fp.Uq, fp.Ud, s_park.sin_t, s_park.cos_t, fp.drive_state);

	// Inverse Clarke
	calculateInvClarke(&s_clarke.Ua, &s_clarke.Ub, &s_clarke.Uc, s_park.Ualfa, s_park.Ubeta);

	}


	s_adc_new  = 1;
	if (NVIC_GetPendingIRQ(ADC1_2_IRQn)) {
	    overrun_count++;
	    __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_JEOS);   // clear peripheral flag
	    NVIC_ClearPendingIRQ(ADC1_2_IRQn);            // then clear NVIC pending
	}
}

// Kalman filter legacy code:
// Perform Kalman filter on angle and speed

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





