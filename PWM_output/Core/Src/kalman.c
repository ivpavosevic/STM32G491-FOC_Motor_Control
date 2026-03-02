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
static int hall_to_sector(uint32_t hall_dec, uint8_t *sector_out) {
  switch (hall_dec) {
    case 1:   *sector_out = 0; return 1; // 001
    case 101: *sector_out = 1; return 1; // 101
    case 100: *sector_out = 2; return 1; // 100
    case 110: *sector_out = 3; return 1; // 110
    case 10:  *sector_out = 4; return 1; // 010
    case 11:  *sector_out = 5; return 1; // 011
    default:  return 0; // invalid (000/111)
  }
}

void HallKF_Init(HallKF *kf, float ts_sec) {
  kf->theta = 0.0f;
  kf->omega = 0.0f;
  kf->p00 = 1.0f;
  kf->p01 = 0.0f;
  kf->p11 = 100.0f;
  kf->ts = ts_sec;

  // Process noise (start conservative for smooth omega)
  kf->q_theta = 1e-5f;
  kf->q_omega = 0.01f;  // tune later; smaller => smoother omega

  // Hall sector quantization variance: (Δθ)^2/12
  kf->r_theta = (SECTOR_STEP * SECTOR_STEP) / 12.0f;
  // omega measurement noise clamp (avoid insane gains at very low speed)
  kf->r_omega_min = 100.0f;

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

void HallKF_Predict(HallKF *kf) {
  // x^- = A x
  kf->theta = wrap_0_2pi(kf->theta + kf->ts * kf->omega);
  // omega_hat(k+1) = omega_hat(k)

  // P^- update in scalar form (A = [[1 Ts],[0 1]])
  const float Ts = kf->ts;
  const float p00 = kf->p00, p01 = kf->p01, p11 = kf->p11;

  kf->p00 = p00 + 2.0f*Ts*p01 + (Ts*Ts)*p11 + kf->q_theta;
  kf->p01 = p01 + Ts*p11;
  kf->p11 = p11 + kf->q_omega;
}

// 1D update for theta: H=[1 0]
static void update_theta(HallKF *kf, float theta_meas) {
// TO DO
}

// 1D update for omega: H=[0 1]
static void update_omega(HallKF *kf, float omega_meas, float r_omega) {
// TO DO
}




