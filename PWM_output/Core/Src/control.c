/*
 *
 *
 *
 * control.c
 *
 *
 * Purpose: PID regulation (Ud, Uq input and Id and Iq feedback),
 * Hall sensors interrupt handlers
 *
 *
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#include "pwm.h"
#include "uart.h"
#include "cordic_sin.h"
#include "control.h"
#include "kalman.h"

static TIM_HandleTypeDef *s_htim_pwm = NULL;

static uint32_t pwm_ch1 = 0;
static uint32_t pwm_ch2 = 0;
static uint32_t pwm_ch3 = 0;

static volatile uint32_t hall_state = 0;

static UART_HandleTypeDef *s_huart = NULL;
static volatile uint32_t pwm_duty = 0;

static volatile uint32_t hall_state_new;
static volatile uint32_t hall_sector_new;
static volatile float hall_angle_new;
volatile float new_Hall_meas_angle = 0.0f; // in radians
volatile uint8_t new_Hall_meas_flag = 0;
volatile uint8_t system_on_off = 0;
volatile int8_t last_hall_step = 0;

volatile uint32_t dwtHALL1stTime = 0;
volatile uint32_t dwtHALL2ndTime = 0;
volatile uint32_t dwtHALLTotalTime = 0;
volatile float dtHALL = 0.0f;
volatile float HALL_omega_est = 0.0f;
volatile float HALL_omega_est_buff = 0.0f;

/* ---- Hall sector-asymmetry compensation ---- */
#define HALL_N            6
#define HALL_CAL_ALPHA    0.01f     // EMA for learning sector widths (~converges in ~50 revs)
#define HALL_CAL_READY    300u      // edges to observe before trusting the table

/* Fix A: full-revolution speed (6 sector times sum to exactly one rev -> asymmetry cancels) */
static float    dtHALL_buf[HALL_N] = {0};
static uint8_t  dtHALL_idx = 0, dtHALL_fill = 0;
volatile float  HALL_omega_rev = 0.0f;          // clean speed telemetry

/* Fix B: per-sector average dwell time -> true edge angles */
static float    sector_dt_ema[HALL_N] = {0};
static uint32_t hall_cal_count = 0;
volatile float  hall_edge_angle[HALL_N] = {     // init to ideal n*60deg
    0.0f, M_PI_3, 2.0f*M_PI_3, 3.0f*M_PI_3, 4.0f*M_PI_3, 5.0f*M_PI_3 };



/*
 * Initialization for Control mechanism - setting up local variables and default values
 */
void Control_Init(TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel,
                  UART_HandleTypeDef *huart) {
  s_htim_pwm = htim_pwm;
  s_huart = huart;
  if (htim_pwm->Instance != TIM1)
    return;

  if (htim_pwm->Instance == TIM1) {
    if (pwm_channel == TIM_CHANNEL_1) {
      pwm_ch1 = pwm_channel;
      pwm_set_duty_percent(s_htim_pwm, pwm_ch1, 0);
    } else if (pwm_channel == TIM_CHANNEL_2) {
      pwm_ch2 = pwm_channel;
      pwm_set_duty_percent(s_htim_pwm, pwm_ch2, 0);
    } else if (pwm_channel == TIM_CHANNEL_3) {
      pwm_ch3 = pwm_channel;
      pwm_set_duty_percent(s_htim_pwm, pwm_ch3, 0);
    }
  }
}

void Setup_Init(FOC_user_params *fp, float theta_0){
	fp->Uq = 0.0f; // 1 means full bus voltage defined by modulation index in PWM ISR (pwm.c)
	fp->Ud = 0.0f;
	fp->rotor_freq = 0.0f;
	fp->Id_ref = 0.0f;
	fp->Iq_ref = 0.0f;
	fp->omega_ref = 628.3f;
	fp->theta = theta_0;
	fp->drive_state = STATE_INIT;

}

void PI_Init_d(PI_reg_t *pi_id){
	pi_id->Ki = 5.0f; // 5
	pi_id->Kp = 0.5f; // 0.5
	pi_id->out_min = -3.0f; // -3
	pi_id->out_max = 3.0f; // 3
	pi_id->sum_err = 0.0f;
	pi_id->sum_err_limit = pi_id->out_max / pi_id->Ki;
	pi_id->dt = 0.0f;
}

void PI_Init_q(PI_reg_t *pi_iq){
	pi_iq->Ki = 7.0f; // 5
	pi_iq->Kp = 0.9f; // 1
	pi_iq->out_min = -5.0f; // -5
	pi_iq->out_max = 5.0f; // -5
	pi_iq->sum_err = 0.0f;
	pi_iq->sum_err_limit = pi_iq->out_max / pi_iq->Ki;
	pi_iq->dt = 0.0f;
}


void PI_Init_omega(PI_reg_t *pi_omega){
	pi_omega->Ki = 0.05f;
	pi_omega->Kp = 0.008f;
	pi_omega->out_min = -0.7f;
	pi_omega->out_max = 0.7f;
	pi_omega->sum_err = 0.0f;
	pi_omega->sum_err_limit = pi_omega->out_max / pi_omega->Ki;
	pi_omega->dt = 0.0f;
}


/**********************
 *
 * Private functions
 *
 * *******************/

/*
 * Reading Hall directly from GPIO
 */
uint32_t readHall(void) {
  uint32_t hall_A, hall_B, hall_C;
  uint32_t hall_output;

  /* HALL_A reading */
  if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == GPIO_PIN_SET) {
    hall_A = 1;
  } else {
    hall_A = 0;
  }

  /* HALL_B reading */
  if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_SET) {
    hall_B = 1;
  } else {
    hall_B = 0;
  }

  /* HALL_C reading */
  if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_SET) {
    hall_C = 1;
  } else {
    hall_C = 0;
  }

  hall_output = hall_A * 100 + hall_B * 10 + hall_C;
  return hall_output;
}

// Map Hall readings to sectors
uint8_t hall_to_sector(uint32_t hall_dec) {
  uint8_t sector_out;
  switch (hall_dec) {
    case 101: sector_out = 0; return sector_out; // 100 -- 001 -- 101 - XOR = 0 // latest: 101
    case 1:   sector_out = 1; return sector_out; // 101 -- 011 -- 001 - XOR = 1 // latest: 001
    case 11:  sector_out = 2; return sector_out; // 001 -- 010 -- 011 - XOR = 0 // latest: 011
    case 10:  sector_out = 3; return sector_out; // 011  -- 110 - 010 - XOR = 1 // latest: 010
    case 110: sector_out = 4; return sector_out; // 010 -- 100 -- 110 - XOR = 0 // latest: 110
    case 100: sector_out = 5; return sector_out; // 110  -- 101 --100 - XOR = 1 // latest: 100
    case 0: return ERR_HALL_INVALID;
    case 111: return ERR_HALL_INVALID;
  }
}

// Map Hall readings to sectors
uint8_t hall_to_sector2(uint32_t hall_dec) {
  uint8_t sector_out;
  switch (hall_dec) {
    case 10: sector_out = 0; return sector_out; // 100 -- 001 -- 101 - XOR = 0
    case 110:   sector_out = 1; return sector_out; // 101 -- 011 -- 001 - XOR = 1
    case 100:  sector_out = 2; return sector_out; // 001 -- 010 -- 011 - XOR = 0
    case 101:  sector_out = 3; return sector_out; // 011  -- 110 - 010 - XOR = 1
    case 1: sector_out = 4; return sector_out; // 010 -- 100 -- 110 - XOR = 0
    case 11: sector_out = 5; return sector_out; // 110  -- 101 --100 - XOR = 1
    case 0: return ERR_HALL_INVALID;
    case 111: return ERR_HALL_INVALID;
  }
}



/*
 * Reading Hall directly from GPIO
 */
uint32_t readAngle(void){
	uint32_t angle_hall_bin = readHall();
	uint32_t angle_deg = 60.0f * hall_to_sector(angle_hall_bin);
	return (angle_deg < 255) ? (uint32_t)angle_deg : 255;
}



float Control_PI_reg(PI_reg_t *pi_x, float err, float dt_s){
	float U_x;

	    pi_x->sum_err += err * dt_s;

	    // Clamp integrator (standard anti-windup)
	    if(pi_x->sum_err > pi_x->sum_err_limit)
	        pi_x->sum_err = pi_x->sum_err_limit;
	    if(pi_x->sum_err < -pi_x->sum_err_limit)
	        pi_x->sum_err = -pi_x->sum_err_limit;

	    U_x = pi_x->Kp * err + pi_x->Ki * pi_x->sum_err;

	    if(U_x >= pi_x->out_max){
	    	U_x = pi_x->out_max;
	    	pi_x->sum_err -= err * dt_s;
	    	return U_x;
	    }
	    if(U_x <= pi_x->out_min){
	    	U_x = pi_x->out_min;
	    	pi_x->sum_err -= err * dt_s;
	    	return U_x;
	    }
	    return U_x;
}

void Drive_ClearFault(void) {
    overcurrent_fault = 0;
    HAL_GPIO_WritePin(GPIOA, Disable_Pin, GPIO_PIN_RESET); /* RESET = enabled (SET = disable) */
    TIM1->BDTR |= TIM_BDTR_MOE;                            /* re-enable PWM outputs            */
}



/*
 * Callback function for pressed 'User' button event
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_13) {
    /* SW debounce */
    static uint32_t last_press_ms = 0;
    uint32_t now = HAL_GetTick();
    char buf4[60];

    /* Ako je tipka stisnuta prebrzo zaredom (<200ms), ignoriraj */
    if ((now - last_press_ms) < 200) {
      return;
    }
    last_press_ms = now;

    /* Iduce linije koda sluze za testiranje da su sinusi pomaknuti u fazama*/
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_6);  // enable
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_11); // disable

    if(system_on_off == 0){
    	system_on_off = 1;
    } else if (system_on_off == 1){
    	system_on_off = 0;
    }



  }/*
  * Interrupt raised every 60°, updates flag for Kalman update step and hall_state
  */
  else if (GPIO_Pin == GPIO_PIN_11 || GPIO_Pin == GPIO_PIN_15 || GPIO_Pin == GPIO_PIN_2){
	dwtHALL2ndTime = DWT->CYCCNT; // Get the cycle value after we had executed our code
	dwtHALLTotalTime = dwtHALL2ndTime - dwtHALL1stTime; // Calculate how many cycles have passed
	dwtHALL1stTime = DWT->CYCCNT;
	dtHALL = convert_ticks_to_s(dwtHALLTotalTime);

	hall_state_new = readHall();
	hall_sector_new = hall_to_sector2(hall_state_new);
	hall_angle_new = M_PI_3 * hall_sector_new;


	if (hall_state_new != hall_state && hall_sector_new != ERR_HALL_INVALID){
		uint8_t sec_new = hall_to_sector2(hall_state_new);
		uint8_t sec_old = hall_to_sector2(hall_state);

		int8_t step = (int8_t)sec_new - (int8_t)sec_old;   // wrap to nearest ±
		if (step >  3) step -= 6;
		if (step < -3) step += 6;

		// Update new hall_state value
		hall_state = hall_state_new;

		// ---- Fix A: full-rev speed (magnitude); sign from current step ----
		dtHALL_buf[dtHALL_idx] = dtHALL;
		dtHALL_idx = (dtHALL_idx + 1) % HALL_N;
		if (dtHALL_fill < HALL_N) dtHALL_fill++;
		if (dtHALL_fill == HALL_N) {
			float t_rev = 0.0f;
			for (uint8_t i = 0; i < HALL_N; i++) t_rev += dtHALL_buf[i];
			if (t_rev > 1e-6f) HALL_omega_rev = (TWO_PI / t_rev) * (float)((step > 0) ? 1 : -1);
		}

		// ---- Fix B: learn true sector widths -> edge-angle table ----
		// dtHALL was spent in the sector we just LEFT (sec_old), regardless of direction
		if (dtHALL > 1e-5f && dtHALL < 0.05f) {                 // only when moving sanely
			sector_dt_ema[sec_old] = (1.0f - HALL_CAL_ALPHA) * sector_dt_ema[sec_old]
			                                + HALL_CAL_ALPHA  * dtHALL;
			if (hall_cal_count < 0xFFFFFFFFu) hall_cal_count++;

			float total = 0.0f;
			for (uint8_t i = 0; i < HALL_N; i++) total += sector_dt_ema[i];
			if (total > 1e-6f) {
				float cum = 0.0f;
				for (uint8_t i = 0; i < HALL_N; i++) {          // edge_angle[i] = start of sector i
					hall_edge_angle[i] = TWO_PI * (cum / total);
					cum += sector_dt_ema[i];
				}
			}
		}

		// the edge just crossed = boundary between sec_old and sec_new:
		//   forward (step>0): start of sec_new ; reverse: start of sec_old
		uint8_t edge_sector = (step > 0) ? sec_new : sec_old;

		if (hall_cal_count >= HALL_CAL_READY)
			new_Hall_meas_angle = hall_edge_angle[edge_sector];   // calibrated
		else
			new_Hall_meas_angle = M_PI_3 * (float)edge_sector;    // ideal until converged

		last_hall_step = step;

		// keep per-edge speed too (for the before/after comparison plot)
		HALL_omega_est_buff = M_PI_3 / dtHALL * step;
		if (HALL_omega_est_buff < 2000.0f && HALL_omega_est_buff >  -2000.0f) HALL_omega_est = HALL_omega_est_buff;
		new_Hall_meas_flag = 1;

	}
  }
}





/********************************************
 *
 *
 *
 * LEGACY CODE - for later purpose
 *
 *
 *
 *******************************************/

/* UART command processor - supports:
 *   pwm <0..100>  - set fixed duty cycle (disables sine generation)
 *   freq <Hz>     - set motor electrical frequency (e.g., "freq 10" = 10 Hz)
 *   mod <0..100>  - set modulation index percentage (e.g., "mod 50" = 0.5)
 */
void process_line(char *line) {
  /* PWM command - set fixed duty cycle (legacy) */
  if (strncmp(line, "pwm", 3) == 0) {
    line += 3;
    while (*line == ' ' || *line == '\t')
      line++;
    int val = (*line) ? atoi(line) : -1;
    if (val < 0)
      val = 0;
    if (val > 100)
      val = 100;

    pwm_duty = (uint32_t)val;
    pwm_set_duty_percent(s_htim_pwm, pwm_ch1, pwm_duty);

    char ack[32];
    int n = snprintf(ack, sizeof(ack), "OK pwm=%d%%\r\n", val);
    HAL_UART_Transmit(s_huart, (uint8_t *)ack, n, HAL_MAX_DELAY);
    return;
  }

//  const char *err = "ERR: use 'freq <Hz>', 'mod <0..100>', 'pwm <0..100>'\r\n";
//  HAL_UART_Transmit(s_huart, (uint8_t *)err, strlen(err), HAL_MAX_DELAY);
}



