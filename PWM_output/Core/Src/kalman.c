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
	if (a >= TWO_PI) a -= TWO_PI;
	if (a < 0.0f)    a += TWO_PI;
  return a;
}

// Wrap difference to [-pi, pi]
static inline float wrap_pm_pi(float a) {
  while (a >  (float)M_PI) a -= TWO_PI;
  while (a < -(float)M_PI) a += TWO_PI;
  return a;
}

// Map Hall readings to sectors
uint32_t hall_to_sector(uint32_t hall_dec) {
  uint8_t sector_out;
  switch (hall_dec) {
    case 100:  sector_out = 0; return sector_out; // 101
    case 101:  sector_out = 1; return sector_out; // 100
    case 1:  sector_out = 2; return sector_out; // 110
    case 11:   sector_out = 3; return sector_out; // 010
    case 10:   sector_out = 4; return sector_out; // 011
    case 110:    sector_out = 5; return sector_out; // 001
    default:  ERR_HALL_FAIL;  // invalid (000/111)
  }
}

void HallKF_Init(HallKF_data *kf, float theta_0) {
  kf->theta = theta_0;
  kf->omega = 0.0f;
  kf->p00 = 1.0f;
  kf->p01 = 0.0f;
  kf->p11 = 3.0f;
  kf->ts = 0;

  // Process noise (start conservative for smooth omega)
  kf->q_theta = 0.0001f;
  kf->q_omega = 0.00005f;

  // Hall sector quantization variance: (Δθ)^2/12
  kf->r_theta = 0.045f;

  kf->K_theta = 0.0f;
  kf->K_omega = 0.0f;

  kf->theta_off = 0.0f;
  kf->ticks_since_edge = 0;
  kf->last_innovation = 0.0f;
  kf->inited = 0;

  s_ev.valid = 0;
}

void CurrVectorKF_Init(CurrVectorKF_data *cvkf, float theta_0) {
  cvkf->theta = theta_0;
  cvkf->omega = 0.0f;
  cvkf->p00 = 1.0f;
  cvkf->p01 = 0.0f;
  cvkf->p02 = 0.0f;
  cvkf->p11 = 10.0f;
  cvkf->p12 = 0.0f;
  cvkf->p22 = 10.0f;
  cvkf->ts = 0;

  // Process noise (start conservative for smooth omega)
  cvkf->q_theta = 0.01f;
  cvkf->q_omega = 0.001f;
  cvkf->q_d_omega = 0.001f;

  cvkf->r_theta = 0.2f;

  cvkf->K_theta = 0.0f;
  cvkf->K_omega = 0.0f;
  cvkf->K_d_omega = 0.0f;

  cvkf->theta_off = 0.0f;
  cvkf->ticks_since_edge = 0;
  cvkf->last_sector = 0;
  cvkf->inited = 0;

  s_ev.valid = 0;
}

// Prediction step every 40us, part of ADC interrupt
void HKF_Predict(HallKF_data *hkf, float dt) {
// -------- predict for rotor Kalman -------------------
	hkf->ts = dt;
	// x^- = A * x
	hkf->theta = wrap_0_2pi(hkf->theta + hkf->ts * hkf->omega);
	// omega_hat(k+1) = omega_hat(k)

	// P^- update in scalar form (A = [[1 Ts],[0 1]]) P^- = A*P*A^T + Q
	float Ts = hkf->ts;
	float p00 = hkf->p00, p01 = hkf->p01, p11 = hkf->p11;

	hkf->p00 = p00 + 2.0f*Ts*p01 + (Ts*Ts)*p11 + hkf->q_theta;
	hkf->p01 = p01 + Ts*p11;
	hkf->p11 = p11 + hkf->q_omega;

	if (hkf->p00 > 2.0f * hkf->r_theta) hkf->p00 = 2.0f * hkf->r_theta;

}

// Update step done when new Hall information is present
void HKF_Update(HallKF_data *hkf, float theta_meas) {
	// update Kalman gain K = (P*H^T)  / (H * P * H^T + R) => K(2x1 matrix)
	hkf->K_theta = hkf->p00 / (hkf->p00 +hkf->r_theta);
	hkf->K_omega = hkf->p01 / (hkf->p00 +hkf->r_theta);

	// Overwrite estimation of theta and omega x = x + K*(Z - H*x);
	/* wrapping in -pi,pi range to avoid huge errors when moving from 2pi to 0 */
	float innovation = wrap_pm_pi(theta_meas - hkf->theta);
	hkf->omega = hkf->omega + hkf->K_omega * innovation; // w_est = w_est + k_w * (θ_meas - θ_est)
	hkf->theta = hkf->theta + hkf->K_theta * innovation; // θ_est = θ_est + k_θ * (θ_meas - θ_est)
	hkf->theta = wrap_0_2pi(hkf->theta);

	// Update covariance matrix P = (I - K*H)*P
	hkf->p00 = (1-hkf->K_theta) * hkf->p00;
	hkf->p11 = hkf->p11 - hkf->K_omega * hkf->p01;
	hkf->p01 = (1-hkf->K_theta) * hkf->p01;


	hkf->last_innovation = innovation;

}

void CVKF_Predict(CurrVectorKF_data *cvkf, float omega_hkf, float dt) {
  // ------ predict for magnetic field Kalman -----------
	cvkf->ts = dt;
	// x^- = A * x
	cvkf->theta = wrap_0_2pi(cvkf->theta + cvkf->ts * cvkf->omega);
	cvkf->omega = omega_hkf + cvkf->d_omega;

	// P^- update in scalar form (A = [[1 Ts],[0 1]]) P^- = A*P*A^T + Q
	float p00 = cvkf->p00;
	float p01 = cvkf->p01;
	float p02 = cvkf->p02;
	float p11 = cvkf->p11;
	float p12 = cvkf->p12;
	float p22 = cvkf->p22;

	// theta
	cvkf->p00 = p00 + 2.0f*cvkf->ts*p01 + (cvkf->ts*cvkf->ts)*p11 + cvkf->q_theta;
	cvkf->p01 = p02 + cvkf->ts*p12;
	cvkf->p02 = p02 + cvkf->ts*p12;

	// omega
	cvkf->p11 = p22 + cvkf->q_omega;
	cvkf->p12 = p22;

	// d_omega
	cvkf->p22 = p22 + cvkf->q_d_omega;

  }


void CVKF_Update(CurrVectorKF_data *cvkf, float theta_meas) {
	// update Kalman gain K = (P*H^T)  / (H * P * H^T + R)
	float S_inv = 1.0f / (cvkf->p00 + cvkf->r_theta);

	cvkf->K_theta  = cvkf->p00 * S_inv;
	cvkf->K_omega  = cvkf->p01 * S_inv;
	cvkf->K_d_omega = cvkf->p02 * S_inv;

	// Inovacija s wrappingom
	float innovation = wrap_pm_pi(theta_meas - cvkf->theta);

	// Update stanja x = x + K * innovation
	cvkf->theta += cvkf->K_theta   * innovation;
	cvkf->omega += cvkf->K_omega  * innovation;
	cvkf->d_omega += cvkf->K_d_omega * innovation;

	cvkf->theta = wrap_0_2pi(cvkf->theta);

	// Update P matrix — (I - K*H) * P
	cvkf->p00 = (1.0f - cvkf->K_theta)  * cvkf->p00;
	cvkf->p01 = (1.0f - cvkf->K_theta)  * cvkf->p01;
	cvkf->p02 = (1.0f - cvkf->K_theta)  * cvkf->p02;

	cvkf->p11 = cvkf->p11 - cvkf->K_omega  * cvkf->p01;
	cvkf->p12 = cvkf->p12 - cvkf->K_omega  * cvkf->p02;

	cvkf->p22 = cvkf->p22 - cvkf->K_d_omega * cvkf->p02;
}





