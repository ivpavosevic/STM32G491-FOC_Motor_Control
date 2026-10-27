/*
 * adc.c
 *
 *
 * Purpose: Init of ADC, raw value conversion and interrupt that calls Clarke & Park transforms
 *
 *  Created on: Jan 20, 2026
 *      Author: ivanp
 */
#include <stdio.h>
#include <string.h>
#include "adc.h"
#include "cordic_sin.h"
#include "kalman.h"
#include "kalman_5x5.h"
#include "control.h"


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

volatile float theta_adc = 0.0f;
volatile float theta_raw = 0.0f;
volatile float theta_measured = 0.0f;
volatile float omega_measured = 0.0f;

static uint32_t dwtADC1stTime = 0;
static uint32_t dwtADC2ndTime = 0;
static uint32_t dwtTotalTime = 0;

static volatile uint16_t offsetA, offsetB, offsetC;

volatile HallKF_data hkf;
volatile CurrVectorKF_data cvkf;
volatile KF5_data k5;            // combined 5-state filter (thesis eq. 3.6-3.7), OBSERVER ONLY
volatile uint8_t use_cv = 0;     // 1 = CVKF accel boosts HKF angle (catch-up), 0 = pure Hall
volatile uint8_t cv_source = 0;  // CVKF measurement: 0 = atan2(current), 1 = atan2(back-EMF)
volatile float theta_emf;        // back-EMF angle atan2(e_beta, e_alpha), [0,2pi)
volatile float e_alpha, e_beta;  // back-EMF components (volts)
volatile float isr_us;           // ISR execution duration in microseconds (debug)
static float dt = 0.0f;

volatile float i_alfa1 = 0.0f;
volatile float i_alfa2 = 0.0f;
volatile float i_beta1 = 0.0f;
volatile float i_beta2 = 0.0f;
volatile float Im, Re = 0.0f;
volatile float theta_v = 0.0f;
volatile float theta_v2 = 0.0f;
volatile float delta_v = 0.0f;
volatile float delta_v2 = 0.0f;
volatile float d_omega_comp = 0.0f;



#define CV_OMEGA_GATE 20.0f      // rad/s elec: below this the measurement is noise -> no boost
#define K5_OMEGA_GATE 0.0f
#define CV_BLANK_CYCLES 38       // ~1.5 ms: blank the CV update after each Hall commutation jump
#define R_S            0.28f     // stator resistance [Ohm] (measured)
#define V_PER_U        2.0f      // phase volts per modulation unit = 0.05*Vdc (Vdc=40)

static float theta_adc_prev     = 0.0f;
volatile float omega_atan2_filt = 0.0f;  // exposed for monitoring
volatile uint32_t counter_cycle_init = 0;
volatile uint32_t counter_cycle_closed_loop = 0;

static int32_t overrun_count = 0;
static uint16_t oc_count = 0;

static int32_t adc_isr_count = 0;
volatile float adc_iq_sum = 0.0f;
volatile float adc_id_sum = 0.0f;

volatile uint32_t steps_since_hall = 0;

volatile float delta_theta = 0.0f;

volatile float acc_est = 0.0f;
volatile float omega_old = 0.0f;

float conv_const = (VREF_V) / (ADC_MAX_VALUE * GAIN * R_SHUNT);

volatile uint8_t overcurrent_fault = 0;
volatile uint8_t cvkf_nan = 0;

static log_sample_t s_log[LOG_SAMPLES];
static volatile uint32_t s_log_idx   = 0;
static volatile uint8_t  s_log_state = 0;      // 0 idle, 1 running, 2 done
static uint32_t s_log_dec = 0;

void    log_arm(void)  { s_log_idx = 0; s_log_dec = 0; s_log_state = 1; }
uint8_t log_done(void) { return s_log_state == 2; }

void log_dump_uart(UART_HandleTypeDef *huart) {
    if (s_log_state != 2) return;                 // only dump a completed capture
    const char *hdr = "theta,omega,innov,iq,id,uq,ud,hall\r\n";
    HAL_UART_Transmit(huart, (uint8_t*)hdr, strlen(hdr), HAL_MAX_DELAY);
    for (uint32_t i = 0; i < LOG_SAMPLES; i++) {
        char line[120];
        int n = snprintf(line, sizeof(line),
            "%.4f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\r\n",
            s_log[i].theta_est, s_log[i].omega, s_log[i].omega_e,
            s_log[i].i_q, s_log[i].d_omega, s_log[i].u_q,
            s_log[i].iq_ref, s_log[i].hall_angle);
        HAL_UART_Transmit(huart, (uint8_t*)line, n, HAL_MAX_DELAY);
    }
    s_log_state = 3;                              // back to idle, ready to re-arm
}

void ADC_Init(ADC_HandleTypeDef *hadc, float theta_0){
	HallKF_Init(&hkf, theta_0);
	CurrVectorKF_Init(&cvkf, theta_0);  // same anchor as HKF — Hall pins absolute angle
	KF5_Init(&k5, theta_0);             // 5x5 observer: same anchor; theta_vs converges via atan2
	// Setup of PI parameters for Id and Iq
	PI_Init_d(&pi_id);
	PI_Init_q(&pi_iq);
	PI_Init_omega(&pi_omega);
	Setup_Init(&fp, theta_0);
	HAL_ADCEx_InjectedStart_IT(hadc);
}

/* Used for debugging purposes to identify duration of interrupt */
void DWT_Init(void) {
	// Enable DWT CYCCNT
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable trace and debug
	DWT->CYCCNT = 0;                                // Reset counter
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Enable the cycle counter
}

float convert_ticks_to_us(uint32_t delta_time){
	return delta_time / 150.0f; // 1 tick = 10^-8 sec * 10^6 to be in usec
}

float convert_ticks_to_s(uint32_t delta_time){
	return delta_time / 1.5e8f; // 1 tick = 10^-8
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

// Wrap to [0, 2*pi)
static inline float wrap_0_2pi_adc(float a) {
	if (a >= TWO_PI) a -= TWO_PI;
	if (a < 0.0f)    a += TWO_PI;
  return a;
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

	sum_ABC = current_A + current_B + current_C;

	// Perform Clarke transform
	calculateClarke(current_A, current_B, current_C, &s_clarke.i_alfa, &s_clarke.i_beta);

	/* Checking duration between interrupts */
	dwtADC2ndTime = DWT->CYCCNT;
	dwtTotalTime  = dwtADC2ndTime - dwtADC1stTime;
	dwtADC1stTime = DWT->CYCCNT;
	dt     = convert_ticks_to_s(dwtTotalTime);


	isr_us = convert_ticks_to_us(dwtTotalTime);

    // theta calculated from current vector
        i_alfa1 = i_alfa2;
        i_beta1 = i_beta2;
        i_alfa2 = s_clarke.i_alfa;
        i_beta2 = s_clarke.i_beta;

        Im = i_alfa1 * i_beta2 - i_beta1 * i_alfa2;
        Re = i_alfa1 * i_alfa2 + i_beta1 * i_beta2;
        delta_v = atan2f(Im, Re);
        delta_v2 = Im / Re;
        theta_v += delta_v;
        theta_v2 += delta_v2;

//		if(new_Hall_meas_flag == 1){
//			theta_v = (float) new_Hall_meas_angle;
//			new_Hall_meas_flag = 0;
//		}

        // wrap to keep within [0, 2*PI]
        if (theta_v >  2*M_PI) theta_v -= 2.0f * M_PI;
        if (theta_v <  0.0f) theta_v += 2.0f * M_PI;
        if (theta_v2 >  2*M_PI) theta_v2 -= 2.0f * M_PI;
        if (theta_v2 <  0.0f) theta_v2 += 2.0f * M_PI;

	theta_raw = atan2_fast(s_clarke.i_beta, s_clarke.i_alfa);
	theta_adc = (theta_raw < 0.0f) ? theta_raw + 2.0f * M_PI : theta_raw;
	if (theta_adc >= 2.0f * M_PI) theta_adc -= 2.0f * M_PI;

//	if(system_on_off == 0){
//		fp.drive_state = STATE_INIT;
//	}


	if (fp.drive_state == STATE_CLOSEDLOOP){
		/* ---- Overcurrent protection: hardware-level kill ---- */
		if (fabsf(current_A) > OVERCURRENT_LIMIT_A ||
		    fabsf(current_B) > OVERCURRENT_LIMIT_A ||
		    fabsf(current_C) > OVERCURRENT_LIMIT_A) {
			if(oc_count++ >= OVERCCURENT_MIN_SAMPLES){
			    TIM1->BDTR &= ~TIM_BDTR_MOE;                          /* kill ALL 6 PWM outputs instantly */
			    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET); /* assert gate-driver disable        */

			    fp.Ud = 0.0f;  fp.Uq = 0.0f;
			    fp.Id_ref = 0.0f;  fp.Iq_ref = 0.0f;
			    fp.drive_state = STATE_INIT;       /* stop the control law from driving */
			    system_on_off = 0;

			    overcurrent_fault = 1;             /* latched — stays tripped until you clear it */
			    return;
			}
		} else{
				oc_count = 0;
		}



		// 1) current-vector Kalman runs FIRST so its acceleration feeds HKF.
		//    Selectable measurement source (cv_source):
		//      0 -> atan2(current)   : follows the command (weak / partly circular)
		//      1 -> atan2(v - R*i)   : back-EMF angle, independent of the command
		//    Uses the PREVIOUS cycle's commanded voltage (s_park.U* written later this ISR).
//		e_alpha = V_PER_U * s_park.Ualfa - R_S * s_clarke.i_alfa;
//		e_beta  = V_PER_U * s_park.Ubeta - R_S * s_clarke.i_beta;
//		float th_e = atan2_fast(e_beta, e_alpha);
//		theta_emf  = (th_e < 0.0f) ? th_e + 2.0f * M_PI : th_e;
//		float cv_meas = (cv_source == 1) ? theta_emf : theta_adc;
		if(fabsf(hkf.omega) > CV_OMEGA_GATE){
			CVKF_Predict(&cvkf, hkf.omega, dt);  // predict ONLY when measured, else P inflates
			CVKF_Update(&cvkf, theta_v);
//			CVKF_Update(&cvkf, theta_adc);
			cvkf_nan = !isfinite(cvkf.theta);
		} else {
			CVKF_Idle(&cvkf, theta_adc, hkf.omega);  // hold at idle; keep P from inflating
		}
		d_omega_comp = cvkf.omega - hkf.omega;

		// 1b) combined 5x5 filter (thesis eq. 3.6-3.7) - OBSERVER ONLY, never commutates.
		//     Same measurement (theta_adc), same low-speed gate. Track in CubeMonitor:
		//     k5.omega_vs, k5.omega_h, k5.d_omega vs hkf.omega / cvkf.omega.
		//     Predict runs every cycle (Hall branch needs it); the CV branch is either
		//     measured or held idle so its covariance cannot inflate while gated off.
		KF5_Predict(&k5, dt);
//		if(fabsf(hkf.omega) > 0.0f){
			KF5_UpdateCV(&k5, theta_v);
//		} else {
//			KF5_IdleCV(&k5, theta_v);
//		}

		// 2) Hall Kalman: theta extrapolates forward using the CVKF acceleration
		//    (catch-up during acceleration).  use_cv = 0 -> pure-Hall baseline.
		float cv_accel = use_cv ? cvkf.d_omega : 0.0f;
		HKF_Predict(&hkf, cvkf.omega, cv_accel, dt);

		// 3) Hall update on a new edge
		if(new_Hall_meas_flag == 1){
			theta_measured = (float) new_Hall_meas_angle;
			omega_measured = (last_hall_step > 0) ? HALL_omega_est : -1.0f * HALL_omega_est;
			HKF_Update(&hkf, theta_measured, last_hall_step );
			KF5_UpdateHall(&k5, theta_measured, last_hall_step);   // 5x5: absolute anchor
			new_Hall_meas_flag = 0;
		}
		fp.theta = k5.theta_h;
//		fp.theta = 0;
//		CORDIC_Update_Angle();


//		if(hkf.omega > 1.0f){
//			float abs_omega = (hkf.omega > 0) ? hkf.omega : -1.0f * hkf.omega;
//			hkf.t_standstill = K_STANDSTILL * M_PI_3 / abs_omega;
//		}


//		counter_cycle_init++;
//
//		if(counter_cycle_init >= 50){
////			log_arm();
//			fp.Uq = 0.6f;
//			counter_cycle_init = 0;// threshold in rad/s
//		}

//		acc_est = (hkf.omega - omega_old) / dt;
//		omega_old = hkf.omega;

//		delta_theta = hkf.theta - cvkf.theta;
//		if(system_on_off == 0){
//			fp.drive_state = STATE_INIT;
//		}

	} else if(fp.drive_state == STATE_INIT && system_on_off == 1){
		fp.theta = hkf.theta_init; // align theta @ measured first angle theta_0
		counter_cycle_init++;
		if(counter_cycle_init >= T_INIT_WAIT){
			fp.Ud = 0.0f;
			fp.Uq = 0.0f;
			fp.Iq_ref = 1.0f;
//			pi_iq.sum_err = fp.Uq / (2.0f*pi_iq.Ki);
//			pi_omega.sum_err = 0.01;

//			log_arm();
			if(overcurrent_fault == 0) fp.drive_state = STATE_CLOSEDLOOP;
			counter_cycle_init = 0;// threshold in rad/s

		}
	}


	CORDIC_CalculateSinCos(fp.theta, &s_park.sin_t, &s_park.cos_t);

	calculatePark(s_clarke.i_alfa, s_clarke.i_beta, fp.theta, &s_park.i_q, &s_park.i_d, s_park.sin_t, s_park.cos_t);

	// PI regulation
	if(fp.drive_state == STATE_CLOSEDLOOP){
		id_err = fp.Id_ref - s_park.i_d;
		iq_err = fp.Iq_ref - s_park.i_q;
		fp.Ud = Control_PI_reg(&pi_id, id_err, dt);
		fp.Uq = Control_PI_reg(&pi_iq, iq_err, dt);
		if(counter_cycle_closed_loop++ >= 1000){
			omega_err = fp.omega_ref - hkf.omega;
//			fp.Iq_ref = Control_PI_reg(&pi_omega, omega_err, dt * 1000);
//			fp.Iq_ref = 0.5f;
			counter_cycle_closed_loop = 0;
		}

	}

	// Inverse Park
	calculateInvPark(&s_park.Ualfa, &s_park.Ubeta, fp.theta, fp.Uq, fp.Ud, s_park.sin_t, s_park.cos_t, fp.drive_state);

	// Inverse Clarke
	calculateInvClarke(&s_clarke.Ua, &s_clarke.Ub, &s_clarke.Uc, s_park.Ualfa, s_park.Ubeta);



	//		adc_isr_count++;
	//		adc_id_sum += s_park.i_d;
	//		adc_iq_sum += s_park.i_q;


//	if (s_log_state == 1) {
//	    if (++s_log_dec >= LOG_DECIMATE) {
//	        s_log_dec = 0;
//	        log_sample_t *p = &s_log[s_log_idx];
//	        p->theta_est  = hkf.theta;
//	        p->omega      = hkf.omega;
//	        p->innovation = hkf.last_innovation;
//	        p->i_q        = s_park.i_q;
//	        p->i_d        = s_park.i_d;
//	        p->u_q        = fp.Uq;
//	        p->p11        = hkf.p11;
//	        p->hall_angle = new_Hall_meas_angle;
//	        if (++s_log_idx >= LOG_SAMPLES) s_log_state = 2;   // full → stop
//	    }
//	}

	}




	s_adc_new  = 1;
//	if (NVIC_GetPendingIRQ(ADC1_2_IRQn)) {
//	    overrun_count++;
//	    __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_JEOS);   // clear peripheral flag
//	    NVIC_ClearPendingIRQ(ADC1_2_IRQn);            // then clear NVIC pending
//	}

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





