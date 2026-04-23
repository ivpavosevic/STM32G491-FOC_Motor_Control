/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#define I2_Pin GPIO_PIN_0
#define I2_GPIO_Port GPIOC
#define I3_Pin GPIO_PIN_1
#define I3_GPIO_Port GPIOC
#define ADC_TRGO_Pin GPIO_PIN_3
#define ADC_TRGO_GPIO_Port GPIOC
#define I1_Pin GPIO_PIN_0
#define I1_GPIO_Port GPIOA
#define Output_Enable_Pin GPIO_PIN_6
#define Output_Enable_GPIO_Port GPIOA
#define HALL_A_Pin GPIO_PIN_4
#define HALL_A_GPIO_Port GPIOC
#define HALL_A_EXTI_IRQn EXTI4_IRQn
#define HALL_B_Pin GPIO_PIN_5
#define HALL_B_GPIO_Port GPIOC
#define HALL_B_EXTI_IRQn EXTI9_5_IRQn
#define L3_Pin GPIO_PIN_1
#define L3_GPIO_Port GPIOB
#define HALL_C_Pin GPIO_PIN_2
#define HALL_C_GPIO_Port GPIOB
#define HALL_C_EXTI_IRQn EXTI2_IRQn
#define L1_Pin GPIO_PIN_13
#define L1_GPIO_Port GPIOB
#define TEMP_Pin GPIO_PIN_14
#define TEMP_GPIO_Port GPIOB
#define Test_pin_Pin GPIO_PIN_8
#define Test_pin_GPIO_Port GPIOC
#define Test_pin2_Pin GPIO_PIN_9
#define Test_pin2_GPIO_Port GPIOC
#define H1_Pin GPIO_PIN_8
#define H1_GPIO_Port GPIOA
#define H2_Pin GPIO_PIN_9
#define H2_GPIO_Port GPIOA
#define H3_Pin GPIO_PIN_10
#define H3_GPIO_Port GPIOA
#define Disable_Pin GPIO_PIN_11
#define Disable_GPIO_Port GPIOA
#define L2_Pin GPIO_PIN_12
#define L2_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
typedef struct{
	float Ud;
	float Uq;
	float rotor_freq;
	float Id_ref;
	float Iq_ref;
	float theta;
} input_params;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
