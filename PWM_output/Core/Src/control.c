/*
 * control.c
 *
 *  Created on: Nov 14, 2025
 *      Author: ivanp
 */

#include "adc.h"
#include "pwm.h"
#include "uart.h"
#include "cordic_sin.h"
#include "control.h"

static TIM_HandleTypeDef *s_htim_pwm = NULL;
static uint32_t pwm_ch1 = 0;
static uint32_t pwm_ch2 = 0;
static uint32_t pwm_ch3 = 0;

static volatile uint32_t counter_hall = 0;
static volatile uint32_t hall_state = 0;
static volatile uint8_t print_flag = 0;

static UART_HandleTypeDef *s_huart = NULL;
static volatile uint32_t pwm_duty_pct = 0;

static int32_t array_states[2*TEST_SIZE];

static volatile uint32_t pwm_duty = 0; // Everything turned off in the start

void Control_Init(TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel,
                  UART_HandleTypeDef *huart) {
  s_htim_pwm = htim_pwm;
  s_huart = huart;
  if (htim_pwm->Instance != TIM1)
    return;

  if (htim_pwm->Instance == TIM1) {
    if (pwm_channel == TIM_CHANNEL_1) {
      pwm_ch1 = pwm_channel;
      pwm_set_duty_percent(s_htim_pwm, pwm_ch1, pwm_duty);
    } else if (pwm_channel == TIM_CHANNEL_2) {
      pwm_ch2 = pwm_channel;
      pwm_set_duty_percent(s_htim_pwm, pwm_ch2, pwm_duty);
    } else if (pwm_channel == TIM_CHANNEL_3) {
      pwm_ch3 = pwm_channel;
      pwm_set_duty_percent(s_htim_pwm, pwm_ch3, pwm_duty);
    }
  }
}

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

  const char *err = "ERR: use 'freq <Hz>', 'mod <0..100>', 'pwm <0..100>'\r\n";
  HAL_UART_Transmit(s_huart, (uint8_t *)err, strlen(err), HAL_MAX_DELAY);
}

uint32_t readHall(void) {
  uint32_t hall_A, hall_B, hall_C;
  uint32_t hall_output;

  /* HALL_A reading */
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4) == GPIO_PIN_SET) {
    hall_A = 1;
  } else {
    hall_A = 0;
  }

  /* HALL_B reading */
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_SET) {
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


int32_t readAngle(void){
	return CORDIC_Get_Angle();
}

int getCounterHallA(void){ return counter_hall; }

/* Callback function for pressed 'User' button event */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_13) {
    /* SW debounce */
    static uint32_t last_press_ms = 0;
    uint32_t now = HAL_GetTick();
    int hallA_read = 0;
    char buf4[60];
    char buf5[60];

    /* Ako je tipka stisnuta prebrzo zaredom (<200ms), ignoriraj */
    if ((now - last_press_ms) < 200) {
      return;
    }
    last_press_ms = now;

    /* Iduce linije koda sluze za testiranje da su sinusi pomaknuti u fazama*/
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_6);  // enable
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_11); // disable


    hallA_read = counter_hall;
    hall_state = readHall();
    int n4;
    int n5;
    if(print_flag){
    	for(uint8_t i = 0; i<= TEST_SIZE; i = i + 2 ) {
    		n4 = snprintf(buf4, sizeof(buf4), "Hall state = --- %03d\r\n", (int)array_states[2 * i]);
            HAL_UART_Transmit(s_huart, (uint8_t *)buf4, n4, HAL_MAX_DELAY);
            n5 = snprintf(buf5, sizeof(buf5), "Angle = --- %03d\r\n", (int)array_states[2*i + 1]);
            HAL_UART_Transmit(s_huart, (uint8_t *)buf5, n5, HAL_MAX_DELAY);
    	}

    }


  } else if (GPIO_Pin == GPIO_PIN_4 || GPIO_Pin == GPIO_PIN_5 || GPIO_Pin == GPIO_PIN_2){
	  int32_t hall_state_new = (int32_t)readHall();
	  int32_t angle_new = readAngle();
	  if (hall_state_new != hall_state){
		  hall_state = hall_state_new;

		  if(counter_hall < TEST_SIZE){
			  array_states[2*counter_hall] = hall_state_new;
			  array_states[2*counter_hall + 1] = angle_new;

		  } else{
			  print_flag = 1;
		  }

		  counter_hall++;
	  }
  }
}
