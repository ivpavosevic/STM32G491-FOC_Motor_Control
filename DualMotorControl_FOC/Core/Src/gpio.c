/*
 * gpio.c
 *
 *  Created on: 16 Jun 2026
 *      Author: ivanp
 */
#include "gpio.h"
/* ============ Private variables ============ */
/* NOTE: most of these are legacy leftovers from the single-motor version and are no longer read anywhere */
static TIM_HandleTypeDef *s_htim_pwm = NULL;

static uint32_t pwm_ch1 = 0;
static uint32_t pwm_ch2 = 0;
static uint32_t pwm_ch3 = 0;

static volatile uint32_t hall_state = 0;
static volatile uint32_t hall_sector_new = 0;


static UART_HandleTypeDef *s_huart = NULL;
static volatile uint32_t pwm_duty = 0;

static volatile uint32_t hall_state_new;
static volatile float hall_angle_new;
volatile float new_Hall_meas_angle = 0.0f; // in radians
volatile uint8_t new_Hall_meas_flag = 0;
volatile uint8_t system_on_off = 0;        // ACTIVE: master run/stop, toggled by the user button
volatile int8_t last_hall_step = 0;

volatile uint32_t dwtHALL1stTime = 0;
volatile uint32_t dwtHALL2ndTime = 0;
volatile uint32_t dwtHALLTotalTime = 0;
volatile float dtHALL = 0.0f;
volatile float HALL_omega_est = 0.0f;
volatile float HALL_omega_est_buff = 0.0f;

/* ============ Motor 2 Hall - software polling ============ */
/* If no commutation for this long, treat the motor as stopped (omega = 0) */
#define HALL_STANDSTILL_TIMEOUT_S 0.2f

/* Global (non-static) so STM32CubeMonitor can watch them live */
volatile uint32_t hall2_state      = 0;    // last VALID composite state
volatile uint32_t hall2_state_raw  = 0;    // latest raw composite (updated every poll)
volatile uint8_t  hall2_sector     = 0;    // 0..5
volatile int8_t   hall2_last_step  = 0;    // +/- direction of last transition
volatile float    hall2_dt         = 0.0f; // seconds between the last two edges
volatile float    hall2_omega_est  = 0.0f; // electrical speed [rad/s]
volatile float    hall2_omega_buff = 0.0f;
volatile float    hall2_meas_angle = 0.0f; // electrical angle [rad]
volatile uint8_t  hall2_meas_flag  = 0;    // set on new edge, clear when consumed

static volatile uint32_t dwtHALL2_prev = 0;
static uint8_t hall2_initialized = 0;

/* ============ Motor 1 Hall - software polling ============ */
/* M1 keeps using the existing consumed globals (hall_state, new_Hall_meas_flag,
   new_Hall_meas_angle, last_hall_step, HALL_omega_est, dtHALL); only the raw
   snapshot + poll timing state are new. */
volatile uint32_t hall1_state_raw = 0;     // latest raw composite (updated every poll)
static volatile uint32_t dwtHALL1_prev = 0;
static uint8_t hall1_initialized = 0;

/* Live pointers to the FOC motor_data, cached in pollHall(), so the Hall EXTI ISR
   can update the correct motor's hall_data in real time. */
static volatile motor_data *s_motor_data_1 = NULL;
static volatile motor_data *s_motor_data_2 = NULL;

/* CubeMX configured the 6 Hall EXTI lines as RISING-edge only. Enabling FALLING
   too (in Hall_Init) lets us catch all 6 commutation edges per electrical rev.
   Lines: PB2, PB3, PB4, PB5, PB11, PB15. NOTE: proper fix is to set these pins to
   "Rising/Falling" in CubeMX and drop this workaround. */
#define HALL_EXTI_FALLING_LINES  ((1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<11)|(1u<<15))


/* ============ Private functions ============ */
/* Reset one motor's Hall state to defaults and enable the falling-edge Hall EXTIs.
 * Input: Mx (which motor), theta_0 (starting angle) */
void Hall_Init(motor_data *Mx, float theta_0){
	Mx->hall_data.hall_state = 0;
	Mx->hall_data.hall_state_raw = 0;
	Mx->hall_data.hall_sector = 0;
	Mx->hall_data.hall_last_step = 0;
	Mx->hall_data.hall_dt = 0.0f;
	Mx->hall_data.hall_dt_prev = 0.0f;
	Mx->hall_data.hall_omega_est = 0.0f;
	Mx->hall_data.hall_omega_buff = 0.0f;
	Mx->hall_data.hall_meas_angle = theta_0;
	Mx->hall_data.hall_new_meas_flag = 0;
	Mx->hall_data.hall_initialized = 0;

	/* Enable falling-edge trigger on the Hall EXTI lines (CubeMX set rising only).
	   Idempotent - safe to run for both M1 and M2 init calls. */
	EXTI->FTSR1 |= HALL_EXTI_FALLING_LINES;
};


/* Read motor 1's 3 Hall pins into one decimal word: A*100 + B*10 + C (e.g. 101). */
uint32_t readHall_M1(void) {
  uint32_t hall_A, hall_B, hall_C;

  hall_A = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == GPIO_PIN_SET) ? 1u : 0u;
  hall_B = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_SET) ? 1u : 0u;
  hall_C = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_SET) ? 1u : 0u;

  return hall_A * 100u + hall_B * 10u + hall_C;
}

/* Same as readHall_M1 but for motor 2's Hall pins. */
uint32_t readHall_M2(void) {
  uint32_t hall_A, hall_B, hall_C;

  hall_A = (HAL_GPIO_ReadPin(HALL_A_2_GPIO_Port, HALL_A_2_Pin) == GPIO_PIN_SET) ? 1u : 0u;
  hall_B = (HAL_GPIO_ReadPin(HALL_B_2_GPIO_Port, HALL_B_2_Pin) == GPIO_PIN_SET) ? 1u : 0u;
  hall_C = (HAL_GPIO_ReadPin(HALL_C_2_GPIO_Port, HALL_C_2_Pin) == GPIO_PIN_SET) ? 1u : 0u;

  return hall_A * 100u + hall_B * 10u + hall_C;
}

/* Map a Hall word (from readHall_Mx) to a 0..5 sector. 000 and 111 are invalid. */
uint8_t hall_to_sector(uint32_t hall_dec) {
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


/* Initial rotor angle estimate from the Hall sector at startup.
 * +PI/6 lands theta in the middle of the current 60 deg sector.
 * Input: motor (1 or 2)   Output: theta_0 in radians */
float read_theta_0(uint8_t motor){
	uint32_t hall_state_0;
	float theta_0;
	if(motor == 1){
		hall_state_0 = readHall_M1();

	} else if (motor == 2){
		hall_state_0 = readHall_M2();
	} else {
		theta_0 = 0; // Make fail detectable
	}

	theta_0 = M_PI_3 *  hall_to_sector(hall_state_0) + M_PI_6; // + PI/6 as approx. theta is half way between two sectors

	return theta_0;
}


///*
// * Reading Hall directly from GPIO
// */
//uint32_t readAngle(void){
//	uint32_t angle_hall_bin = readHall();
//	uint32_t angle_deg = 60.0f * hall_to_sector(angle_hall_bin);
//	return (angle_deg < 255) ? (uint32_t)angle_deg : 255;
//}

/*
 * Process one Hall commutation edge for a motor. Called from the Hall EXTI ISR
 * (HAL_GPIO_EXTI_Callback). Re-reads the full 3-bit Hall word and, on a genuine
 * sector change, updates angle/step/speed and raises the new-measurement flag the
 * Kalman update in the ADC ISR consumes. Same logic as the former pollHall() edge
 * branch, just triggered by the pin edge instead of by polling.
 */
static void Hall_ProcessEdge(volatile motor_data *Mx, uint8_t motor) {
  if (Mx == NULL) return;                        /* pointer not cached yet */

  uint32_t s = (motor == 1) ? readHall_M1() : readHall_M2();
  Mx->hall_data.hall_state_raw = s;

  uint8_t sec_new = hall_to_sector(s);
  if (sec_new == ERR_HALL_INVALID) return;       /* ignore 000 / 111 glitch */

  /* First valid edge: seed state only, don't compute a bogus dt/step */
  if (!Mx->hall_data.hall_initialized) {
    Mx->hall_data.hall_state       = s;
    Mx->hall_data.hall_dt_prev     = DWT->CYCCNT;
    Mx->hall_data.hall_initialized = 1;
    return;
  }

  if (s != Mx->hall_data.hall_state) {           /* genuine 60 deg transition */
    uint32_t now = DWT->CYCCNT;
    Mx->hall_data.hall_dt      = convert_ticks_to_s(now - Mx->hall_data.hall_dt_prev);
    Mx->hall_data.hall_dt_prev = now;

    uint8_t sec_old = hall_to_sector(Mx->hall_data.hall_state);
    int8_t step = (int8_t)sec_new - (int8_t)sec_old;   // wrap to nearest +/-
    if (step >  3) step -= 6;
    if (step < -3) step += 6;

    /* The edge just crossed = boundary between sec_old and sec_new. Which boundary
       depends on direction: forward (step>0) -> start of sec_new; reverse -> start
       of sec_old. Using a fixed sec_new gives a 60 deg error on reverse crossings,
       which makes the dither-freeze snap theta between two positions. */
    uint8_t edge_sector = (step > 0) ? sec_new : sec_old;

    Mx->hall_data.hall_state      = s;
    Mx->hall_data.hall_meas_angle = M_PI_3 * edge_sector;
    Mx->hall_data.hall_last_step  = step;

    if (Mx->hall_data.hall_dt > 0.0f) {
      Mx->hall_data.hall_omega_buff = M_PI_3 / Mx->hall_data.hall_dt * step;
      if (Mx->hall_data.hall_omega_buff < 2000.0f && Mx->hall_data.hall_omega_buff > -2000.0f) {
        Mx->hall_data.hall_omega_est = Mx->hall_data.hall_omega_buff;
      }
    }
    Mx->hall_data.hall_new_meas_flag = 1;         // new measurement ready for Kalman/control
  }
}

/*
 * Called once per control tick from the ADC ISR. Edge detection now lives in the
 * Hall EXTI ISR; here we only (1) cache the live motor_data pointer so the EXTI ISR
 * can reach this motor's hall_data, and (2) handle standstill - EXTI cannot fire
 * while the rotor is stopped, so if no edge has arrived for too long we force the
 * Hall speed estimate to zero.
 */
void pollHall(motor_data *Mx, uint8_t motor) {
  if (motor == 1)      s_motor_data_1 = Mx;
  else if (motor == 2) s_motor_data_2 = Mx;

  if (Mx->hall_data.hall_initialized &&
      convert_ticks_to_s(DWT->CYCCNT - Mx->hall_data.hall_dt_prev) > HALL_STANDSTILL_TIMEOUT_S) {
    Mx->hall_data.hall_omega_est = 0.0f;
  }
}

/* ============ Interrupt functions ============ */

/*
 * Shared EXTI callback: 6 Hall lines (both motors) + user button. Each Hall pin
 * sits on its own EXTI line, so we dispatch purely by pin number.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  switch (GPIO_Pin) {

  /* Motor 1 Hall: PB15 (A), PB11 (B), PB2 (C)*/
  case HALL_A_1_from_PC4_Pin:
  case HALL_B_1_from_PC5_Pin:
  case HALL_C_1_Pin:
    Hall_ProcessEdge(s_motor_data_1, 1u);
    break;

  /* Motor 2 Hall: PB3 (A), PB4 (B), PB5 (C)*/
  case HALL_A_2_Pin:
  case HALL_B_2_Pin:
  case HALL_C_2_Pin:
    Hall_ProcessEdge(s_motor_data_2, 2u);
    break;

  /* User button PC13: toggle system on/off + gate-driver enables */
  case GPIO_PIN_13: {
    static uint32_t last_press_ms = 0;
    uint32_t now = HAL_GetTick();
    if ((now - last_press_ms) < 200u) return;    /* debounce */
    last_press_ms = now;

    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_6);   // enable 1
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_11);  // disable 1
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_6);   // enable 2
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_10);  // disable 2

    system_on_off = (system_on_off == 0u) ? 1u : 0u;
    break;
  }

  default:
    break;
  }
}
