/*
 * kalman.c
 *
 *	Purpose: Kalman filter for angle and speed estimation based on Hall sensors input
 *
 *
 *  Created on: Feb 26, 2026
 *      Author: ivanp
 */
#include "kalman.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#define TWO_PI (2.0f * (float)M_PI)
#define SECTOR_STEP ( (float)M_PI / 3.0f ) // 60 deg electrical
#define Q31_SCALE 2147483648.0f

typedef struct {
  volatile uint16_t dt_ticks;
  volatile uint8_t  sector;
  volatile uint8_t  valid;
} HallEvent;

static HallEvent s_ev = {0};

// Wrap to [0, 2*pi)
static inline float wrap_0_2pi(float a) {
  while (a >= TWO_PI) a -= TWO_PI;
  while (a < 0.0f)    a += TWO_PI;
  return a;
}

// Wrap difference to [-pi, pi]
static inline float wrap_pm_pi(float a) {
  while (a >  (float)M_PI) a -= TWO_PI;
  while (a < -(float)M_PI) a += TWO_PI;
  return a;
}

// Map Hall readings to sectors
uint16_t hall_to_sector(uint32_t hall_dec) {
  uint8_t sector_out;
  switch (hall_dec) {
    case 101:  sector_out = 0; return sector_out; // 101
    case 100:  sector_out = 1; return sector_out; // 100
    case 110:  sector_out = 2; return sector_out; // 110
    case 10:   sector_out = 3; return sector_out; // 010
    case 11:   sector_out = 4; return sector_out; // 011
    case 1:    sector_out = 5; return sector_out; // 001
    default:  return 255; // invalid (000/111)
  }
}

void HallKF_Init(HallKF *kf, uint16_t theta_0) {
  kf->theta = (float) theta_0;
  kf->omega = 0.0f;
  kf->p00 = 1.0f;
  kf->p01 = 0.0f;
  kf->p11 = 100.0f;
  kf->ts = 0;

  // Process noise (start conservative for smooth omega)
  kf->q_theta = 1e-5f;
  kf->q_omega = 0.001f;  // tune later; smaller => smoother omega

  // Hall sector quantization variance: (Δθ)^2/12
  kf->r_theta = (SECTOR_STEP * SECTOR_STEP) / 12.0f;

  kf->K_theta = 0.0f;
  kf->K_omega = 0.0f;

  kf->theta_off = 0.0f;
  kf->ticks_since_edge = 0;
  kf->last_sector = 0;
  kf->inited = 0;

  s_ev.valid = 0;
}

void HallKF_Tick40us(HallKF *kf) {
  kf->ticks_since_edge++;
}

// for HALL EXTI ISR
void HallKF_OnHallEdgeGPIO(HallKF *kf, uint32_t hall_state_dec) {
// TO DO - behaviour on HALL ISR
}


// Prediction step every 40us, part of ADC interrupt
void KF_Predict(HallKF *kf, float dt) {
  kf->ts = dt;
  // x^- = A * x
  kf->theta = wrap_0_2pi(kf->theta + kf->ts * kf->omega);
  // omega_hat(k+1) = omega_hat(k)

  // P^- update in scalar form (A = [[1 Ts],[0 1]]) P^- = A*P*A^T + Q
  const float Ts = kf->ts;
  const float p00 = kf->p00, p01 = kf->p01, p11 = kf->p11;

  kf->p00 = p00 + 2.0f*Ts*p01 + (Ts*Ts)*p11 + kf->q_theta;
  kf->p01 = p01 + Ts*p11;
  kf->p11 = p11 + kf->q_omega;
}

// Update step done when new Hall information is present
void KF_Update(HallKF *kf, float theta_meas) {
 // update Kalman gain K = (P*H^T)  / (H * P * H^T + R) => K(2x1 matrix)
  kf->K_theta = kf->p00 / (kf->p00 +kf->r_theta);
  kf->K_omega = kf->p01 / (kf->p00 +kf->r_theta);


  // Overwrite estimation of theta and omega x = x + K*(Z - H*x);
  kf->theta = kf->theta + kf->K_theta * (theta_meas - kf->theta); // θ_est = θ_est + k_θ * (θ_meas - θ_est)
  kf->omega = kf->omega + kf->K_omega * (theta_meas - kf->theta); // w_est = w_est + k_w * (θ_meas - θ_est)

  // Update covariance matrix P = (I - K*H)*P
  kf->p00 = (1-kf->K_theta) * kf->p00;
  kf->p11 = kf->p11 - kf->K_omega * kf->p01;
  kf->p01 = (1-kf->K_theta) * kf->p01;

}





