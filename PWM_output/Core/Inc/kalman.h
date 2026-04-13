/*
 * kalman.h
 *
 *  Created on: Feb 26, 2026
 *      Author: ivanp
 */

#ifndef INC_KALMAN_H_
#define INC_KALMAN_H_

#include "main.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define ERR_HALL_FAIL -1

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

void HallKF_Init(HallKF *kf, float theta_0);

void KF_Predict(HallKF *kf, float dt);
void KF_Update(HallKF *kf, float theta_meas);

uint32_t hall_to_sector(uint32_t hall_dec);

#endif /* INC_KALMAN_H_ */
