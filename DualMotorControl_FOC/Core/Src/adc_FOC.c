/*
 * adc_FOC.c
 *
 *  Created on: 15 Jun 2026
 *      Author: ivanp
 */

#include "adc_FOC.h"
#include "kalman.h"
#include "pi_reg.h"
#include "gpio.h"
#include "cordic.h"

/* ============ Private variables ============ */
static volatile motor_data M1;
static volatile motor_data M2;


/* Variables used for current reading from ADC */
static volatile uint8_t  s_adc_new  = 0;
static volatile uint8_t  s_adc_calibrated  = 0;

static volatile uint8_t  M1_s_adc_calibrated  = 0;
static volatile uint8_t  M2_s_adc_calibrated  = 0;

volatile float M1_id_err = 0.0f;
volatile float M1_iq_err = 0.0f;
volatile float M2_id_err = 0.0f;
volatile float M2_iq_err = 0.0f;

volatile float M1_omega_err = 0.0f;
volatile float M2_omega_err = 0.0f;


static float theta_adc = 0.0f;
static float theta_raw = 0.0f;
static volatile float M1_theta_measured = 0.0f;
static volatile float M2_theta_measured = 0.0f;
static float omega_measured = 0.0f;

static uint32_t dwtADC1stTime = 0;
static uint32_t dwtADC2ndTime = 0;
static uint32_t dwtTotalTime = 0;

volatile float dt_us = 0.0f;   // loop period in us (telemetry)
static float dt = 0.0f;        // loop period in s (Kalman + PI use this)
volatile uint32_t counter_cycle_init = 0;
volatile uint32_t counter_cycle_closed_loop = 0;

static int32_t overrun_count = 0;
static uint16_t oc_count = 0;             // overcurrent debounce counter
volatile uint8_t overcurrent_fault = 0;   // latched: stays tripped until cleared


static int32_t adc_isr_count = 0;
volatile float adc_iq_sum = 0.0f;
volatile float adc_id_sum = 0.0f;

volatile uint32_t steps_since_hall = 0;

volatile float delta_theta = 0.0f;

volatile float acc_est = 0.0f;
volatile float omega_old = 0.0f;

volatile uint8_t M1_use_cv = 0;     // 1 = CVKF accel boosts HKF angle (catch-up), 0 = pure Hall
volatile uint8_t M2_use_cv = 0;     // 1 = CVKF accel boosts HKF angle (catch-up), 0 = pure Hall


/* current-vector angle bits: prev/now alfa-beta, Im/Re, running angle */
volatile float M1_i_alfa1 = 0.0f;
volatile float M1_i_alfa2 = 0.0f;
volatile float M1_i_beta1 = 0.0f;
volatile float M1_i_beta2 = 0.0f;
volatile float M1_Im, M1_Re = 0.0f;
volatile float M1_theta_v = 0.0f;
volatile float M1_delta_v = 0.0f;
volatile float M2_i_alfa1 = 0.0f;
volatile float M2_i_alfa2 = 0.0f;
volatile float M2_i_beta1 = 0.0f;
volatile float M2_i_beta2 = 0.0f;
volatile float M2_Im, M2_Re = 0.0f;
volatile float M2_theta_v = 0.0f;
volatile float M2_delta_v = 0.0f;
volatile float M1_theta_adc = 0.0f;
volatile float M2_theta_adc = 0.0f;
volatile float M1_d_omega_comp = 0.0f;
volatile float M2_d_omega_comp = 0.0f;


float conv_const = (VREF_V) / (ADC_MAX_VALUE * GAIN * R_SHUNT);

static log_sample_t s_log[LOG_SAMPLES];
static volatile uint32_t s_log_idx   = 0;
static volatile uint8_t  s_log_state = 0;      // 0 idle, 1 running, 2 done
static uint32_t s_log_dec = 0;

ADC_HandleTypeDef *s_hadc = NULL;

/* ============ Private functions ============ */
void    log_arm(void)  { s_log_idx = 0; s_log_dec = 0; s_log_state = 1; }
uint8_t log_done(void) { return s_log_state == 2; }

void log_dump_uart(UART_HandleTypeDef *huart) {
    if (s_log_state != 2) return;                 // only dump a completed capture
    const char *hdr = "theta,omega,innov,iq,id,uq,p11,hall\r\n";
    HAL_UART_Transmit(huart, (uint8_t*)hdr, strlen(hdr), HAL_MAX_DELAY);
    for (uint32_t i = 0; i < LOG_SAMPLES; i++) {
        char line[120];
        int n = snprintf(line, sizeof(line),
            "%.4f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\r\n",
            s_log[i].theta_est, s_log[i].omega, s_log[i].innovation,
            s_log[i].i_q, s_log[i].i_d, s_log[i].u_q,
            s_log[i].p11, s_log[i].hall_angle);
        HAL_UART_Transmit(huart, (uint8_t*)line, n, HAL_MAX_DELAY);
    }
    s_log_state = 3;                              // back to idle, ready to re-arm
}

/* default FOC params for one motor at startup */
void Setup_Init(foc_user_params *fp, float theta_0){
	fp->Uq = 0.0f; // 1 means full bus voltage defined by modulation index in PWM ISR (pwm.c)
	fp->Ud = 0.0f;
	fp->rotor_freq = 0.0f;
	fp->Id_ref = 0.0f;
	fp->Iq_ref = 0.0f;
	fp->omega_ref = 300.0f;
	fp->theta = theta_0;
	fp->drive_state = STATE_INIT;

}

/* set up Hall, Kalman, PI and FOC for both motors, then start the ADC */
void ADC_FOC_Init(ADC_HandleTypeDef *hadc, float M1_theta_0, float M2_theta_0){
	/* Hall measurement init*/
	Hall_Init(&M1, M1_theta_0);
	Hall_Init(&M2, M2_theta_0);

	/* Kalman filter setup*/
	HallKF_Init(&M1.hkf, M1_theta_0);
	HallKF_Init(&M2.hkf, M2_theta_0);

	CurrVectorKF_Init(&M1.cvkf, M1_theta_0);
	CurrVectorKF_Init(&M2.cvkf, M2_theta_0);

	KF5_Init(&M1.k5, M1_theta_0);
	KF5_Init(&M2.k5, M2_theta_0);

	/* PI regulation setup*/
	PI_Init_d(&M1.pi_id);
	PI_Init_q(&M1.pi_iq);
	PI_Init_omega(&M1.pi_omega);

	PI_Init_d(&M2.pi_id);
	PI_Init_q(&M2.pi_iq);
	PI_Init_omega(&M2.pi_omega);

	/* FOC parameters setup*/
	Setup_Init(&M1.fp, M1_theta_0);
	Setup_Init(&M2.fp, M2_theta_0);

	s_hadc = hadc;
	HAL_ADCEx_InjectedStart_IT(s_hadc);
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

/* polynomial atan approx, core (|x| <= 1) */
static inline float atan_approx_core(float x) {
    float x2 = x * x;
    return x * (0.99997726f + x2 * (-0.33262347f + x2 * (0.19354346f +
           x2 * (-0.11643287f + x2 * (0.05265332f + x2 * -0.01172120f)))));
}

/* fast atan2, good to ~1e-3 rad */
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
float ADC_ConvRawCurrValue(uint16_t raw_v, uint8_t phase, uint8_t motor){
	// Convert voltage reading to current with R_shunt = 0.001 Ohm
	int16_t raw_v_offs = 0;
	if(phase == 1){
		raw_v_offs = (motor == 1) ? (raw_v - M1.adc_conv.offset_A) : (raw_v - M2.adc_conv.offset_A);
	} else if(phase == 2) {
		raw_v_offs = (motor == 1) ? (raw_v - M1.adc_conv.offset_B) : (raw_v - M2.adc_conv.offset_B);
	} else if (phase == 3){
		raw_v_offs = (motor == 1) ? (raw_v - M1.adc_conv.offset_C) : (raw_v - M2.adc_conv.offset_C);
	}
	float conv_v = raw_v_offs * conv_const;
	return conv_v;
}

/* copy latest raw currents out if a new sample is ready */
uint8_t ADC1_PopCurrentsValues(adc_conv_t *out, motor_data *Mx)
{
    if (!out) return 0;

    if (!s_adc_new) return 0;

    out->ia_raw = Mx->adc_conv.ia_raw;
    out->ib_raw = Mx->adc_conv.ib_raw;
    out->ic_raw = Mx->adc_conv.ic_raw;
    s_adc_new = 0;
    return 1;
}

/* Used during intialization when to make an average of all offsets for reading zero A */
void ADC_StartCalibration1(void){
	adc_conv_t adc_cal_data1;
	uint32_t sumA1 = 0;
	uint32_t sumB1 = 0;
//	uint32_t sumC1 = 0;
	uint16_t c1 = 0;
	HAL_Delay(100);

	while (c1 < ADC_CAL_SIZE1 ){
		if(s_adc_new == 1){
			ADC1_PopCurrentsValues(&adc_cal_data1, &M1);
			sumA1 += adc_cal_data1.ia_raw;
			sumB1 += adc_cal_data1.ib_raw;
//			sumC1 += adc_cal_data1.ic_raw;
			c1++;
		}
	}


	M1.adc_conv.offset_A = sumA1 / ADC_CAL_SIZE1;
	M1.adc_conv.offset_B = sumB1 / ADC_CAL_SIZE1;
//	M1.adc_conv.offset_C = sumC1 / ADC_CAL_SIZE1;

	M1_s_adc_calibrated = 1;
}

void ADC_StartCalibration2(void){
	adc_conv_t adc_cal_data2;
	uint32_t sumA2 = 0;
	uint32_t sumB2 = 0;
	uint32_t sumC2 = 0;
	uint16_t c2 = 0;
	HAL_Delay(100);

	while (c2 < ADC_CAL_SIZE2 ){
		if(s_adc_new == 1){
			ADC1_PopCurrentsValues(&adc_cal_data2, &M2);
			sumA2 += adc_cal_data2.ia_raw;
			sumB2 += adc_cal_data2.ib_raw;
			sumC2 += adc_cal_data2.ic_raw;
			c2++;
		}
	}


	M2.adc_conv.offset_A = sumA2 / ADC_CAL_SIZE2;
	M2.adc_conv.offset_B = sumB2 / ADC_CAL_SIZE2;
	M2.adc_conv.offset_C = sumC2 / ADC_CAL_SIZE2;

	M2_s_adc_calibrated = 1;
}

/*
 * Updating voltage levels on three-phase gates
 */
void Get3PhaseV_ABC(float *Ua, float *Ub, float *Uc, uint8_t motor) {

	if(motor == 1){
	__disable_irq();
	*Ua = M1.s_clarke.Ua;
	*Ub = M1.s_clarke.Ub;
	*Uc = M1.s_clarke.Uc;
	__enable_irq();
 } else if (motor == 2){
	 __disable_irq();
	*Ua = M2.s_clarke.Ua;
	*Ub = M2.s_clarke.Ub;
	*Uc = M2.s_clarke.Uc;
	__enable_irq();
 } else{
	 __disable_irq();
	 *Ua = 0.0f;
	 *Ub = 0.0f;
	 *Uc = 0.0f;
	 __enable_irq();
 }

}

/* phase over the limit for N samples -> kill both inverters. returns 1 on trip */
uint8_t overcurrent_protection(void){
	/* ---- Overcurrent protection: hardware-level kill ---- */
	if (fabsf(M1.adc_conv.i_a) > OVERCURRENT_LIMIT ||
	    fabsf(M1.adc_conv.i_b) > OVERCURRENT_LIMIT ||
	    fabsf(M1.adc_conv.i_c) > OVERCURRENT_LIMIT ||
		fabsf(M2.adc_conv.i_a) > OVERCURRENT_LIMIT ||
		fabsf(M2.adc_conv.i_b) > OVERCURRENT_LIMIT ||
		fabsf(M2.adc_conv.i_c) > OVERCURRENT_LIMIT ) {
		if(oc_count++ >= OVERCCURENT_MIN_SAMPLES){
		    TIM1->BDTR &= ~TIM_BDTR_MOE;                          /* kill ALL 6 PWM outputs instantly */
		    TIM8->BDTR &= ~TIM_BDTR_MOE;
		    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET); /* assert gate-driver disable        */
		    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET); /* assert gate-driver disable        */

		    M1.fp.Ud = 0.0f;  M1.fp.Uq = 0.0f;
		    M1.fp.Id_ref = 0.0f;  M1.fp.Iq_ref = 0.0f;
		    M1.fp.drive_state = STATE_INIT;       /* stop the control law from driving */

		    M2.fp.Ud = 0.0f;  M2.fp.Uq = 0.0f;
		    M2.fp.Id_ref = 0.0f;  M2.fp.Iq_ref = 0.0f;
		    M2.fp.drive_state = STATE_INIT;       /* stop the control law from driving */
		    system_on_off = 0;

		   return 1;
		} else {
			return 0;
		}
	} else{
			oc_count = 0;
			return 0;
	}
}

/* ============ Interrupt functions ============ */

/* Interrupt for reading the ADC values and perforimg Clarke and Parke transforms (every 40 us)*/
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{

	if (hadc->Instance != ADC1)
	return;


	/* Software polling of both motors' Hall sensors */
	pollHall(&M1, 1); // 1 for M1
	pollHall(&M2, 2); // 2 for M2

	/* Direct JDR reads (ranks 1-4 = M2 ib, M2 ic, M1 ia, M1 ib; no injected offset configured) */
	M2.adc_conv.ib_raw = (uint16_t)ADC1->JDR1;
	M2.adc_conv.ic_raw = (uint16_t)ADC1->JDR2;
	M1.adc_conv.ia_raw = (uint16_t)ADC1->JDR3;
	M1.adc_conv.ib_raw = (uint16_t)ADC1->JDR4;


	if(M1_s_adc_calibrated == 1 && M2_s_adc_calibrated == 1){
	// Transform raw to readable data
	M1.adc_conv.i_a = -ADC_ConvRawCurrValue(M1.adc_conv.ia_raw, 1, 1u);
	M1.adc_conv.i_b = -ADC_ConvRawCurrValue(M1.adc_conv.ib_raw, 2, 1u);
//	M1.adc_conv.i_c = -ADC_ConvRawCurrValue(M1.adc_conv.ic_raw, 3, 1u);
	M1.adc_conv.i_c = -(M1.adc_conv.i_a + M1.adc_conv.i_b);


//	M2.adc_conv.i_a = -ADC_ConvRawCurrValue(M2.adc_conv.ia_raw, 1, 2u);
	M2.adc_conv.i_b = -ADC_ConvRawCurrValue(M2.adc_conv.ib_raw, 2, 2u);
	M2.adc_conv.i_c = -ADC_ConvRawCurrValue(M2.adc_conv.ic_raw, 3, 2u);
	M2.adc_conv.i_a = -(M2.adc_conv.i_b + M2.adc_conv.i_c);

	// Perform Clarke transform
//	__disable_irq();
	calculateClarke(M1.adc_conv.i_a, M1.adc_conv.i_b, M1.adc_conv.i_c, &M1.s_clarke.i_alfa, &M1.s_clarke.i_beta);
	calculateClarke(M2.adc_conv.i_a, M2.adc_conv.i_b, M2.adc_conv.i_c, &M2.s_clarke.i_alfa, &M2.s_clarke.i_beta);
//	__enable_irq();

    // current-vector angle from successive samples (small angle: delta ~ Im/Re)
    M1_i_alfa1 = M1_i_alfa2;
    M1_i_beta1 = M1_i_beta2;
    M1_i_alfa2 = M1.s_clarke.i_alfa;
    M1_i_beta2 = M1.s_clarke.i_beta;

    M1_Im = M1_i_alfa1 * M1_i_beta2 - M1_i_beta1 * M1_i_alfa2;
    M1_Re = M1_i_alfa1 * M1_i_alfa2 + M1_i_beta1 * M1_i_beta2;
//    M1_delta_v = atan2f(M1_Im, M1_Re);
//    M1_theta_v += M1_delta_v;
//    M1_theta_v = wrap_0_2pi_adc(M1_theta_v);

    if (M1_Re > DELTA_RE_MIN) {
        M1_delta_v = M1_Im / M1_Re;
        if (M1_delta_v >  DELTA_MAX) M1_delta_v =  DELTA_MAX;
        if (M1_delta_v < -DELTA_MAX) M1_delta_v = -DELTA_MAX;
        M1_theta_v += M1_delta_v;
        M1_theta_v = wrap_0_2pi_adc(M1_theta_v);
    }

    M2_i_alfa1 = M2_i_alfa2;
    M2_i_beta1 = M2_i_beta2;
    M2_i_alfa2 = M2.s_clarke.i_alfa;
    M2_i_beta2 = M2.s_clarke.i_beta;

    M2_Im = M2_i_alfa1 * M2_i_beta2 - M2_i_beta1 * M2_i_alfa2;
    M2_Re = M2_i_alfa1 * M2_i_alfa2 + M2_i_beta1 * M2_i_beta2;
//    M2_delta_v = atan2f(M2_Im, M2_Re);
//    M2_theta_v += M2_delta_v;
//    M2_theta_v = wrap_0_2pi_adc(M2_theta_v);

    if (M2_Re > DELTA_RE_MIN) {
        M2_delta_v = M2_Im / M2_Re;
        if (M2_delta_v >  DELTA_MAX) M2_delta_v =  DELTA_MAX;
        if (M2_delta_v < -DELTA_MAX) M2_delta_v = -DELTA_MAX;
        M2_theta_v += M2_delta_v;
        M2_theta_v = wrap_0_2pi_adc(M2_theta_v);
    }

//	M1_theta_adc = atan2_fast(M1.s_clarke.i_beta, M2.s_clarke.i_alfa);
//	M1_theta_adc = (M1_theta_adc < 0.0f) ? M1_theta_adc + 2.0f * M_PI : M1_theta_adc;
//	if (M1_theta_adc >= 2.0f * M_PI) M1_theta_adc -= 2.0f * M_PI;
//
//	M2_theta_adc = atan2_fast(M1.s_clarke.i_beta, M2.s_clarke.i_alfa);
//	M2_theta_adc = (M2_theta_adc < 0.0f) ? M2_theta_adc + 2.0f * M_PI : M2_theta_adc;
//	if (M2_theta_adc >= 2.0f * M_PI) M2_theta_adc -= 2.0f * M_PI;
//
//	/* Checking duration between interrupts */
	dwtADC2ndTime = DWT->CYCCNT; // Get the cycle value after we had executed our code
	dwtTotalTime = dwtADC2ndTime - dwtADC1stTime; // Calculate how many cycles have passed
	dwtADC1stTime = DWT->CYCCNT;
	dt = convert_ticks_to_s(dwtTotalTime);
	dt_us  = convert_ticks_to_us(dwtTotalTime);

	if (system_on_off == 1 && M1.fp.drive_state == STATE_CLOSEDLOOP && M2.fp.drive_state == STATE_CLOSEDLOOP){
		overcurrent_fault = overcurrent_protection();
		if(overcurrent_fault == 1){
			Error_Handler();
		}
//		if(fabsf(M1.hkf.omega) > CV_OMEGA_GATE){
//			CVKF_Predict(&M1.cvkf, M1.hkf.omega, dt);  // predict ONLY when measured, else P inflates
//			CVKF_Update(&M1.cvkf, M1_theta_v);
////			CVKF_Update(&cvkf, theta_adc);
//		} else {
//			CVKF_Idle(&M1.cvkf, M1_theta_adc, M1.hkf.omega);  // hold at idle; keep P from inflating
//		}
//		if(fabsf(M2.hkf.omega) > CV_OMEGA_GATE){
//			CVKF_Predict(&M2.cvkf, M2.hkf.omega, dt);  // predict ONLY when measured, else P inflates
//			CVKF_Update(&M2.cvkf, M2_theta_v);
////			CVKF_Update(&cvkf, theta_adc);
//		} else {
//			CVKF_Idle(&M2.cvkf, M2_theta_adc, M2.hkf.omega);  // hold at idle; keep P from inflating
//		}
//		M1_d_omega_comp = M1.cvkf.omega - M1.hkf.omega;
//		M2_d_omega_comp = M2.cvkf.omega - M2.hkf.omega;
//
//		// 2) Hall Kalman: theta extrapolates forward using the CVKF acceleration
//		//    (catch-up during acceleration).  use_cv = 0 -> pure-Hall baseline.
//		float M1_cv_accel = M1_use_cv ? M1.cvkf.d_omega : 0.0f;
//		HKF_Predict(&M1.hkf, M1.cvkf.omega, M1_cv_accel, dt);
//
//		float M2_cv_accel = M2_use_cv ? M2.cvkf.d_omega : 0.0f;
//		HKF_Predict(&M2.hkf, M2.cvkf.omega, M2_cv_accel, dt);

//		HKF_Predict(&M1.hkf, 0.0f, 0.0f, dt);
//		HKF_Predict(&M2.hkf, 0.0f, 0.0f, dt);
		KF5_Predict(&M1.k5, dt);
		KF5_UpdateCV(&M1.k5, M1_theta_v);

		KF5_Predict(&M2.k5, dt);
		KF5_UpdateCV(&M2.k5, M2_theta_v);

		// 2nd step: Kalman update - done every time new Hall data appears.
		// Snapshot the shared (volatile) hall_data into locals once: the higher-priority
		// Hall EXTI can preempt this block, so re-reading hall_meas_angle would risk a
		// torn read (different edge for the telemetry copy vs the HKF_Update arg).
		if(M1.hall_data.hall_new_meas_flag == 1){
			float  m1_hall_angle = M1.hall_data.hall_meas_angle;
			int8_t m1_hall_step  = M1.hall_data.hall_last_step;
			M1.hall_data.hall_new_meas_flag = 0;              // clear before use: a new edge re-arms it
			M1_theta_measured = m1_hall_angle;
//			HKF_Update(&M1.hkf, m1_hall_angle, m1_hall_step);
			KF5_UpdateHall(&M1.k5, M1_theta_measured, m1_hall_step);   // 5x5: absolute anchor

		}
		if(M2.hall_data.hall_new_meas_flag == 1){
			float  m2_hall_angle = M2.hall_data.hall_meas_angle;
			int8_t m2_hall_step  = M2.hall_data.hall_last_step;
			M2.hall_data.hall_new_meas_flag = 0;
			M2_theta_measured = m2_hall_angle;
//			HKF_Update(&M2.hkf, m2_hall_angle, m2_hall_step);
			KF5_UpdateHall(&M2.k5, m2_hall_angle, m2_hall_step);   // 5x5: absolute anchor

		}
		M1.fp.theta = M1.k5.theta_h;
		M2.fp.theta = M2.k5.theta_h;

	// startup: wait a bit, then set Iq_ref and switch both motors to closed loop
	} else if(system_on_off == 1 && M1.fp.drive_state == STATE_INIT && M2.fp.drive_state == STATE_INIT){
		if(counter_cycle_init++ >= T_INIT_WAIT){
//			log_arm();

			if(overcurrent_fault == 0){
				M1.fp.Ud = 0.0f;
				M1.fp.Uq = 0.0f;
				M1.fp.Iq_ref = 1.0f;

				M2.fp.Ud = 0.0f;
				M2.fp.Uq = 0.0f;
				M2.fp.Iq_ref = -1.0f;

				M1.fp.drive_state = STATE_CLOSEDLOOP;
				M2.fp.drive_state = STATE_CLOSEDLOOP;
			}
			counter_cycle_init = 0;// threshold in rad/s
		}
	}


	// sin/cos of the estimated angle, for the Park transforms
	CORDIC_CalculateSinCos(M1.fp.theta, &M1.s_park.sin_t, &M1.s_park.cos_t);
	CORDIC_CalculateSinCos(M2.fp.theta, &M2.s_park.sin_t, &M2.s_park.cos_t);

//	__disable_irq();
	calculatePark(M1.s_clarke.i_alfa, M1.s_clarke.i_beta, M1.fp.theta, &M1.s_park.i_q, &M1.s_park.i_d, M1.s_park.sin_t, M1.s_park.cos_t);
	calculatePark(M2.s_clarke.i_alfa, M2.s_clarke.i_beta, M2.fp.theta, &M2.s_park.i_q, &M2.s_park.i_d, M2.s_park.sin_t, M2.s_park.cos_t);
//	__enable_irq();

	// PI regulation
	if(M1.fp.drive_state == STATE_CLOSEDLOOP && M2.fp.drive_state == STATE_CLOSEDLOOP && system_on_off == 1){
		M1_id_err = M1.fp.Id_ref - M1.s_park.i_d;
		M1_iq_err = M1.fp.Iq_ref - M1.s_park.i_q;
		M1.fp.Ud = Control_PI_reg(&M1.pi_id, M1_id_err, dt);
		M1.fp.Uq = Control_PI_reg(&M1.pi_iq, M1_iq_err, dt);

		M2_id_err = M2.fp.Id_ref - M2.s_park.i_d;
		M2_iq_err = M2.fp.Iq_ref - M2.s_park.i_q;
		M2.fp.Ud = Control_PI_reg(&M2.pi_id, M2_id_err, dt);
		M2.fp.Uq = Control_PI_reg(&M2.pi_iq, M2_iq_err, dt);

		if(counter_cycle_closed_loop++ >= 1000){
//			M1_omega_err = M1.fp.omega_ref - M1.hkf.omega;
//			M2_omega_err = M2.fp.omega_ref - M2.hkf.omega;
//			M1.fp.Iq_ref = Control_PI_reg(&M1.pi_omega, M1_omega_err, dt * 1000);
//			M1.fp.Iq_ref = Control_PI_reg(&M1.pi_omega, M1_omega_err, dt * 1000);
			counter_cycle_closed_loop = 0;
		}

	}

		if(system_on_off == 1){
//			__disable_irq();
			// Inverse Park
			calculateInvPark(&M1.s_park.Ualfa, &M1.s_park.Ubeta, M1.fp.theta, M1.fp.Uq, M1.fp.Ud, M1.s_park.sin_t, M1.s_park.cos_t, M1.fp.drive_state);
			calculateInvPark(&M2.s_park.Ualfa, &M2.s_park.Ubeta, M2.fp.theta, M2.fp.Uq, M2.fp.Ud, M2.s_park.sin_t, M2.s_park.cos_t, M2.fp.drive_state);

			// Inverse Clarke
			calculateInvClarke(&M1.s_clarke.Ua, &M1.s_clarke.Ub, &M1.s_clarke.Uc, M1.s_park.Ualfa, M1.s_park.Ubeta);
			calculateInvClarke(&M2.s_clarke.Ua, &M2.s_clarke.Ub, &M2.s_clarke.Uc, M2.s_park.Ualfa, M2.s_park.Ubeta);
//			__enable_irq();

	}

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

	// switched off while running -> stop and re-init from the current angle
	if(system_on_off == 0 && M1.fp.drive_state == STATE_CLOSEDLOOP && M2.fp.drive_state == STATE_CLOSEDLOOP){
		M1.fp.drive_state = STATE_INIT;
		M2.fp.drive_state = STATE_INIT;
		ADC_FOC_Init(s_hadc, M1.fp.theta, M2.fp.theta);
	}
//	if (NVIC_GetPendingIRQ(ADC1_2_IRQn)) {
//	    overrun_count++;
//	    __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_JEOS);   // clear peripheral flag
//	    NVIC_ClearPendingIRQ(ADC1_2_IRQn);            // then clear NVIC pending
//	}

}

