/*
 * kalman.h
 *
 *  Created on: Feb 26, 2026
 *      Author: ivanp
 */

#ifndef INC_KALMAN_H_
#define INC_KALMAN_H_

#include "main.h"

typedef struct {
  float theta;   // rad, electrical, wrapped to [0, 2*pi)
  float omega;   // rad/s, electrical
  float p00, p01, p11; // covariance
  float q_theta, q_omega;
  float r_theta;
  float ts;
  float theta_off;
  float K_theta;
  float K_omega;
  uint32_t ticks_since_edge;
  uint8_t  last_sector;
  uint8_t  inited;
} HallKF;

void HAllKF_Init(HallKF *kf, uint16_t theta_0);

void HallKF_Tick40us(HallKF *kf); // call each ADC ISR tick
void HallKF_OnHallEdgeGPIO(HallKF *kf, uint32_t hall_state_dec); // call in EXTI ISR (fast)

void KF_Predict(HallKF *kf, float dt);
void KF_Update(HallKF *kf, float theta_meas);

uint16_t hall_to_sector(uint32_t hall_dec);

#endif /* INC_KALMAN_H_ */
