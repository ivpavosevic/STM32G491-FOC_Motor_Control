/*
 * gpio.h
 *
 *  Created on: 16 Jun 2026
 *      Author: ivanp
 */
#ifndef INC_GPIO_H_
#define INC_GPIO_H_

#include "main.h"
#include "adc_FOC.h"

#define ERR_HALL_INVALID 255u

extern volatile uint8_t new_Hall_meas_flag;
extern volatile float new_Hall_meas_angle;
extern volatile uint8_t system_on_off;
extern volatile int8_t last_hall_step;
extern volatile float HALL_omega_est;

/* Motor 1 Hall - software polling (raw snapshot watchable in CubeMonitor) */
extern volatile uint32_t hall1_state_raw;

/* Motor 2 Hall - software polling (watchable in CubeMonitor) */
extern volatile uint32_t hall2_state;
extern volatile uint32_t hall2_state_raw;
extern volatile uint8_t  hall2_sector;
extern volatile int8_t   hall2_last_step;
extern volatile float    hall2_dt;
extern volatile float    hall2_omega_est;
extern volatile float    hall2_meas_angle;
extern volatile uint8_t  hall2_meas_flag;

void Hall_Init(motor_data *Mx, float theta_0);
uint32_t readHall_M1(void);
uint32_t readHall_M2(void);
uint8_t hall_to_sector(uint32_t hall_dec);
float read_theta_0(uint8_t motor);
uint32_t readAngle(void);
void pollHall(motor_data *Mx, uint8_t motor);


#endif /* INC_GPIO_H_ */
