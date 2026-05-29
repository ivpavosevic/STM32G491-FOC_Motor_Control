/*
 * kalman.h
 *
 *  Created on: Feb 26, 2026
 *      Author: ivanp
 */

#ifndef INC_KALMAN_H_
#define INC_KALMAN_H_

#include "main.h"

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
  float  last_innovation;
  uint8_t  inited;
} HallKF_data;

typedef struct {
  float theta;   // rad, electrical, wrapped to [0, 2*pi)
  float omega;   // rad/s, electrical
  float d_omega;
  float p00, p01, p02, p11, p12, p22; // covariance
  float q_theta;
  float q_omega;
  float q_d_omega;
  float r_theta;
  float ts;
  float theta_off;
  float K_theta;
  float K_omega;
  float K_d_omega;
  uint32_t ticks_since_edge;
  uint8_t  last_sector;
  uint8_t  inited;
} CurrVectorKF_data;

void HallKF_Init(HallKF_data *kf, float theta_0);
void CurrVectorKF_Init(CurrVectorKF_data *kf, float theta_0);

void HKF_Predict(HallKF_data *hkf, float dt);
void CVKF_Predict(CurrVectorKF_data *cvkf, float omega_hkf, float dt);

void HKF_Update(HallKF_data *hkf, float theta_meas);
void CVKF_Update(CurrVectorKF_data *cvkf, float theta_meas);


uint32_t hall_to_sector(uint32_t hall_dec);

#endif /* INC_KALMAN_H_ */
