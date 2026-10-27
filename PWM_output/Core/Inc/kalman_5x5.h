/*
 * kalman_5x5.h
 *
 *  Spojeni 5-stanjski linearni Kalmanov filter (Hall + vektor struje).
 *  Combined 5-state linear Kalman filter (Hall + current vector), thesis eq. (3.6)-(3.7).
 *
 *  Stanje / state:  x = [theta_vs, omega_vs, theta_h, omega_h, d_omega]
 *    theta_vs  - kut vektora struje = atan2(i_beta, i_alfa)   [0,2pi)
 *    omega_vs  - brzina vektora struje
 *    theta_h   - elektricni kut rotora (Hall)                 [0,2pi)  -> izlaz za FOC
 *    omega_h   - brzina rotora = omega_vs + d_omega
 *    d_omega   - odstupanje brzina = omega_h - omega_vs
 *
 *  A = [ 1  Ts  0   0   0 ] [ 0   1  0   0   0 ] [ 0   0  1   Ts  0 ]
 *      [ 0   1  0   0   1 ] [ 0   0  0   0   1 ]
 *  Mjerenja: z_vs = theta_vs (H=[1 0 0 0 0]),  z_h = theta_h (H=[0 0 1 0 0]).
 */

#ifndef INC_KALMAN_5X5_H_
#define INC_KALMAN_5X5_H_

#include "main.h"

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

void KF5_Init(KF5_data *kf, float theta_0);
void KF5_Predict(KF5_data *kf, float dt);
void KF5_UpdateCV(KF5_data *kf, float theta_vs_meas);             /* atan2 vektor struje */
void KF5_UpdateHall(KF5_data *kf, float theta_hall, int8_t step); /* Hall brid */
void KF5_IdleCV(KF5_data *kf, float theta_vs_seed);              /* hold CV grane dok je zatvoreno */

#endif /* INC_KALMAN_5X5_H_ */
