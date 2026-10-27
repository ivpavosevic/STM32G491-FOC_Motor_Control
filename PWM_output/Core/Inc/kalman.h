/*
 * kalman.h
 *
 *  Created on: Feb 26, 2026
 *      Author: ivanp
 */

#ifndef INC_KALMAN_H_
#define INC_KALMAN_H_

#include "main.h"

#define ERR_HALL_INVALID 255u

#define HKF_STANDSTILL_INIT 15000.0f // if no moving for 1s

#define P11_AT_REST  10.0f
#define P_MIN 1e-6f

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

void HallKF_Init(HallKF_data *kf, float theta_0);
void CurrVectorKF_Init(CurrVectorKF_data *kf, float theta_0);

void HKF_Predict(HallKF_data *hkf, float cvkf_omega, float cvkf_d_omega, float dt);
void CVKF_Predict(CurrVectorKF_data *cvkf, float omega_hkf, float dt);

void HKF_Update(HallKF_data *hkf, float theta_meas, int8_t step);
void CVKF_Update(CurrVectorKF_data *cvkf, float theta_atan2);
void CVKF_Idle(CurrVectorKF_data *cvkf, float theta_seed, float omega_seed); // hold while gated off



#endif /* INC_KALMAN_H_ */
