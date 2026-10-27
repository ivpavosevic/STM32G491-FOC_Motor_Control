/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define I2_1_Pin GPIO_PIN_0
#define I2_1_GPIO_Port GPIOC
#define I3_1_Pin GPIO_PIN_1
#define I3_1_GPIO_Port GPIOC
#define I2_2_new_Pin GPIO_PIN_2
#define I2_2_new_GPIO_Port GPIOC
#define ADC_TRGO_1_Pin GPIO_PIN_3
#define ADC_TRGO_1_GPIO_Port GPIOC
#define I1_1_Pin GPIO_PIN_0
#define I1_1_GPIO_Port GPIOA
#define I3_2_Pin GPIO_PIN_1
#define I3_2_GPIO_Port GPIOA
#define LED2_Pin GPIO_PIN_5
#define LED2_GPIO_Port GPIOA
#define Output_Enable_1_Pin GPIO_PIN_6
#define Output_Enable_1_GPIO_Port GPIOA
#define L3_1_Pin GPIO_PIN_1
#define L3_1_GPIO_Port GPIOB
#define HALL_C_1_Pin GPIO_PIN_2
#define HALL_C_1_GPIO_Port GPIOB
#define HALL_C_1_EXTI_IRQn EXTI2_IRQn
#define Disable_2_Pin GPIO_PIN_10
#define Disable_2_GPIO_Port GPIOB
#define HALL_B_1_from_PC5_Pin GPIO_PIN_11
#define HALL_B_1_from_PC5_GPIO_Port GPIOB
#define HALL_B_1_from_PC5_EXTI_IRQn EXTI15_10_IRQn
#define TEMP_2_Pin GPIO_PIN_12
#define TEMP_2_GPIO_Port GPIOB
#define L1_1_Pin GPIO_PIN_13
#define L1_1_GPIO_Port GPIOB
#define TEMP_1_Pin GPIO_PIN_14
#define TEMP_1_GPIO_Port GPIOB
#define HALL_A_1_from_PC4_Pin GPIO_PIN_15
#define HALL_A_1_from_PC4_GPIO_Port GPIOB
#define HALL_A_1_from_PC4_EXTI_IRQn EXTI15_10_IRQn
#define Test_pin_Pin GPIO_PIN_8
#define Test_pin_GPIO_Port GPIOC
#define Test_pin2_Pin GPIO_PIN_9
#define Test_pin2_GPIO_Port GPIOC
#define H1_1_Pin GPIO_PIN_8
#define H1_1_GPIO_Port GPIOA
#define H2_1_Pin GPIO_PIN_9
#define H2_1_GPIO_Port GPIOA
#define H3_1_Pin GPIO_PIN_10
#define H3_1_GPIO_Port GPIOA
#define Disable_1_Pin GPIO_PIN_11
#define Disable_1_GPIO_Port GPIOA
#define L2_1_Pin GPIO_PIN_12
#define L2_1_GPIO_Port GPIOA
#define HALL_A_2_Pin GPIO_PIN_3
#define HALL_A_2_GPIO_Port GPIOB
#define HALL_A_2_EXTI_IRQn EXTI3_IRQn
#define HALL_B_2_Pin GPIO_PIN_4
#define HALL_B_2_GPIO_Port GPIOB
#define HALL_B_2_EXTI_IRQn EXTI4_IRQn
#define HALL_C_2_Pin GPIO_PIN_5
#define HALL_C_2_GPIO_Port GPIOB
#define HALL_C_2_EXTI_IRQn EXTI9_5_IRQn
#define OE_2_Pin GPIO_PIN_6
#define OE_2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#ifndef M_PI_2
#define M_PI_2  1.57079632679489661923f
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef M_PI_3
#define M_PI_3 1.04719755119659774615f
#endif

#ifndef M_PI_6
#define M_PI_6 0.52359877559829887308f
#endif

#ifndef TWO_PI
#define TWO_PI (2.0f * (float)M_PI)
#endif


typedef struct{
	uint32_t hall_state;
	uint32_t hall_state_raw;
	uint8_t hall_sector;
	int8_t hall_last_step;
	float hall_dt;
	float hall_dt_prev;
	float hall_omega_est;
	float hall_omega_buff;
	float hall_meas_angle;
	uint8_t hall_new_meas_flag;
	uint8_t hall_initialized;
} hall_data_t;

typedef struct {
  float theta;   // rad, electrical, wrapped to [0, 2*pi)
  float theta_init;
  float omega;   // rad/s, electrical
  float p00, p01, p11; // covariance
  float q_theta, q_omega;
  float r_theta;
  float r_omega;
  float ts;
  float t_standstill;
  float K_theta;
  float K_omega;
  uint32_t ticks_since_edge;
  float  last_innovation;
  uint8_t  inited;
  float last_theta_meas;
  float second_last_theta_meas;
  uint8_t dither_count;
  int8_t last_step;
} HallKF_data;

typedef struct {
  /* 3-state constant-acceleration filter, x = [theta, omega, d_omega] */
  float theta;    // atan2-estimated angle, rad, [0, 2pi)
  float omega;    // angular speed of the current vector, rad/s
  float d_omega;  // angular ACCELERATION dω/dt, rad/s^2  (drives HKF angle catch-up)
  /* covariance P (3x3 symmetric, upper triangle: 0=theta 1=omega 2=d_omega) */
  float p00, p01, p02;
  float      p11, p12;
  float           p22;
  float q_theta;
  float q_omega;
  float q_d_omega;
  float r_atan2;  // atan2 angle measurement variance — tune on bench
  float ts;
  float K_theta;
  float K_omega;
  float K_d_omega;
  float last_innovation;
} CurrVectorKF_data;

typedef struct {
  /* stanje x = [theta_vs, omega_vs, theta_h, omega_h, d_omega] */
  float theta_vs;
  float omega_vs;
  float theta_h;     /* izlaz za FOC */
  float omega_h;     /* = omega_vs + d_omega */
  float d_omega;     /* = omega_h - omega_vs */

  /* kovarijanca P (5x5 simetricna, 15 gornjih clanova) */
  float p00, p01, p02, p03, p04;
  float      p11, p12, p13, p14;
  float           p22, p23, p24;
  float                p33, p34;
  float                     p44;

  /* procesni sum Q (dijagonala) */
  float q_theta_vs, q_omega_vs, q_theta_h, q_omega_h, q_d_omega;
  /* mjerni sum R */
  float r_vs;        /* varijanca kuta vektora struje */
  float r_h;         /* varijanca Hall kuta */

  float ts;
  /* Hall: mirovanje + anti-dither */
  float    t_standstill;
  uint32_t ticks_since_edge;
  uint8_t  dither_count;
  int8_t   last_step;
  float    last_innovation;
} KF5_data;

typedef struct{
	float Kp;
	float Ki;
	float sum_err;
	float sum_err_limit;
	float out_min;
	float out_max;
	float dt;
} PI_reg_t;

typedef enum {
	STATE_INIT,
	STATE_CLOSEDLOOP,
	STATE_CALIBRATE
} drive_state_t;

typedef enum {
	SYSTEM_ON,
	SYSTEM_OFF
} system_state_t;

typedef struct {
    uint16_t ia_raw;
    uint16_t ib_raw;
    uint16_t ic_raw;
    uint16_t offset_A;
    uint16_t offset_B;
    uint16_t offset_C;
    float i_a;
    float i_b;
    float i_c;
} adc_conv_t;

typedef struct{
	float Ud;
	float Uq;
	float theta;
	float rotor_freq;
	float Id_ref;
	float Iq_ref;
	float omega_ref;
	drive_state_t drive_state;
} foc_user_params;

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

typedef struct {
    float theta_est;     // hkf.theta
    float omega;         // hkf.omega
    float innovation;    // hkf.last_innovation
    float i_q;           // s_park.i_q
    float i_d;           // s_park.i_d
    float u_q;           // fp.Uq
    float p11;           // hkf.p11
    float hall_angle;    // new_Hall_meas_angle
} log_sample_t;          // 8 floats = 32 B/sample


typedef struct {
	foc_user_params fp;
	adc_conv_t adc_conv;
	foc_params_clarke s_clarke;
	foc_params_park s_park;
	log_sample_t log_data;
	HallKF_data hkf;
	CurrVectorKF_data cvkf;
	KF5_data k5;
	PI_reg_t pi_id;
	PI_reg_t pi_iq;
	PI_reg_t pi_omega;
	hall_data_t hall_data;
} motor_data;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
