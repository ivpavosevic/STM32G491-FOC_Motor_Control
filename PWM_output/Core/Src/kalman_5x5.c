/*
 * kalman_5x5.c
 *
 *  Spojeni 5-stanjski linearni Kalmanov filter (Hall + vektor struje).
 *  Combined 5-state linear Kalman filter (Hall + current vector), per thesis eq. (3.6)-(3.7).
 *
 *  Stanje / state:  x = [theta_vs, omega_vs, theta_h, omega_h, d_omega]
 *    theta_vs  - kut vektora struje  = atan2(i_beta, i_alfa)        [0,2pi)
 *    omega_vs  - brzina vektora struje
 *    theta_h   - elektricni kut rotora (Hall)                       [0,2pi)  -> izlaz za FOC
 *    omega_h   - brzina rotora (Hall) = omega_vs + d_omega
 *    d_omega   - odstupanje brzina = omega_h - omega_vs
 *
 *  A = [ 1  Ts  0   0   0 ]   theta_vs += Ts*omega_vs
 *      [ 0   1  0   0   0 ]   omega_vs  = omega_vs            (random walk)
 *      [ 0   0  1   Ts  0 ]   theta_h  += Ts*omega_h
 *      [ 0   1  0   0   1 ]   omega_h   = omega_vs + d_omega  (vezano stanje!)
 *      [ 0   0  0   0   1 ]   d_omega   = d_omega             (random walk)
 *
 *  Mjerenja / measurements:
 *    z_vs = theta_vs (atan2),  H_vs = [1 0 0 0 0]   - svaki ciklus (r_vs)
 *    z_h  = theta_h  (Hall),   H_h  = [0 0 1 0 0]   - samo na Hall bridu (r_h)
 *
 *  NAPOMENA: ovo je samostalni modul. Da bi se zvao iz adc.c, prebaci typedef
 *  KF5_data i 4 prototipa nize u zaglavlje (npr. kalman_5x5.h).
 *
 *  Created on: 2026
 *      Author: ivanp
 */
#include "kalman_5x5.h"
#include "kalman.h"   /* za P11_AT_REST, HKF_STANDSTILL_INIT */
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Wrap na [0, 2*pi) */
static inline float wrap_0_2pi(float a) {
  if (a >= TWO_PI) a -= TWO_PI;
  if (a < 0.0f)    a += TWO_PI;
  return a;
}
/* Wrap razlike na (-pi, pi] */
static inline float wrap_pm_pi(float a) {
  while (a >  (float)M_PI) a -= TWO_PI;
  while (a < -(float)M_PI) a += TWO_PI;
  return a;
}

void KF5_Init(KF5_data *kf, float theta_0) {
  kf->theta_vs = theta_0;  kf->omega_vs = 0.0f;
  kf->theta_h  = theta_0;  kf->omega_h  = 0.0f;
  kf->d_omega  = 0.0f;

  /* P: kutovi okvirno poznati, brzine/odstupanje nepoznati, bez pocetne korelacije */
  kf->p00 = 2.5f; kf->p01 = 0.0f; kf->p02 = 0.0f; kf->p03 = 0.0f; kf->p04 = 0.0f;
                  kf->p11 = 10.0f; kf->p12 = 0.0f; kf->p13 = 0.0f; kf->p14 = 0.0f;
                                   kf->p22 = 0.1f; kf->p23 = 0.0f; kf->p24 = 0.0f;
                                                   kf->p33 = 10.0f; kf->p34 = 0.0f;
                                                                    kf->p44 = 10.0f;

  /* Q (dijagonala) --- PODESITI --- */
  kf->q_theta_vs = 1e-4f;
  kf->q_omega_vs = 0.01f;   /* CV grana je konst-brzina (nema stanja ubrzanja) -> treba brz
                              random walk da sustigne 40V ubrzanje bez zaostajanja kuta */
  kf->q_theta_h  = 0.0001f;
  kf->q_omega_h  = 1e-2f;  /* omega_h je vezano stanje (=omega_vs+d_omega); mali Q>0
                              drzi P nesingularnom (0 radi, ali je numericki na rubu) */
  kf->q_d_omega  = 0.05f;

  /* R --- PODESITI --- */
  kf->r_vs = 0.25f;         /* atan2 vektora struje je sumovit */
  kf->r_h  = 0.1f;         /* Hall kvantizacija (pi/3)^2/12 ~ 0.09 */

  kf->ts = 0.0f;
  kf->t_standstill     = HKF_STANDSTILL_INIT;
  kf->ticks_since_edge = 0;
  kf->dither_count     = 0;
  kf->last_step        = 0;
  kf->last_innovation  = 0.0f;
}

/* Predikcija svaki ciklus.  x^- = A x,  P^- = A P A^T + Q */
void KF5_Predict(KF5_data *kf, float dt) {
  kf->ts = dt;
  kf->ticks_since_edge++;

  if (kf->ticks_since_edge > kf->t_standstill) {
    /* nema Hall brida predugo -> mirovanje: nuliraj brzine, resetiraj kovarijancu */
    kf->omega_vs = 0.0f; kf->omega_h = 0.0f; kf->d_omega = 0.0f;
    kf->p01 = 0.0f; kf->p03 = 0.0f; kf->p04 = 0.0f;
    kf->p12 = 0.0f; kf->p13 = 0.0f; kf->p14 = 0.0f;
    kf->p23 = 0.0f; kf->p24 = 0.0f; kf->p34 = 0.0f;
    kf->p11 = P11_AT_REST; kf->p33 = P11_AT_REST; kf->p44 = P11_AT_REST;
    return;
  }

  float Ts = dt, Ts2 = dt*dt;

  /* --- stanje --- */
  kf->theta_vs = wrap_0_2pi(kf->theta_vs + Ts * kf->omega_vs);
  kf->theta_h  = wrap_0_2pi(kf->theta_h  + Ts * kf->omega_h);
  kf->omega_h  = kf->omega_vs + kf->d_omega;   /* A redak 4: omega_h = omega_vs + d_omega */
  /* omega_vs, d_omega ostaju (random walk) */

  /* --- kovarijanca: snimi sve stare clanove pa propagiraj --- */
  float p00 = kf->p00, p01 = kf->p01, p02 = kf->p02, p03 = kf->p03, p04 = kf->p04;
  float                p11 = kf->p11, p12 = kf->p12, p13 = kf->p13, p14 = kf->p14;
  float                              p22 = kf->p22, p23 = kf->p23, p24 = kf->p24;
  float                                             p33 = kf->p33, p34 = kf->p34;
  float                                                            p44 = kf->p44;

  kf->p00 = p00 + 2.0f*Ts*p01 + Ts2*p11 + kf->q_theta_vs;
  kf->p01 = p01 + Ts*p11;
  kf->p02 = p02 + Ts*(p03 + p12) + Ts2*p13;
  kf->p03 = p01 + p04 + Ts*(p11 + p14);
  kf->p04 = p04 + Ts*p14;
  kf->p11 = p11 + kf->q_omega_vs;
  kf->p12 = p12 + Ts*p13;
  kf->p13 = p11 + p14;
  kf->p14 = p14;
  kf->p22 = p22 + 2.0f*Ts*p23 + Ts2*p33 + kf->q_theta_h;
  kf->p23 = p12 + p24 + Ts*(p13 + p34);
  kf->p24 = p24 + Ts*p34;
  kf->p33 = p11 + 2.0f*p14 + p44 + kf->q_omega_h;
  kf->p34 = p14 + p44;
  kf->p44 = p44 + kf->q_d_omega;
}

/* Idle hold za granu vektora struje dok je mjerenje zatvoreno (niska brzina).
 * KF5_Predict se i dalje vrti (Hallova grana ga treba), ali bez CV korekcije P
 * grane vektora struje neograniceno raste -> re-seat na init svaki idle ciklus.
 * Hallov blok (theta_h=2, omega_h=3) se cuva. */
void KF5_IdleCV(KF5_data *kf, float theta_vs_seed) {
  kf->theta_vs = theta_vs_seed;
  kf->omega_vs = kf->omega_h;   /* sjeme brzine vektora struje = Hall brzina */
  kf->d_omega  = 0.0f;
  /* kovarijanca CV stanja (0=theta_vs, 1=omega_vs, 4=d_omega) natrag na init;
     krizni clanovi prema Hall bloku na 0 (grane razdvojene dok je CV zatvoren) */
  kf->p00 = 2.5f;  kf->p11 = 10.0f;  kf->p44 = 10.0f;
  kf->p01 = 0.0f;  kf->p02 = 0.0f;  kf->p03 = 0.0f;  kf->p04 = 0.0f;
  kf->p12 = 0.0f;  kf->p13 = 0.0f;  kf->p14 = 0.0f;
  kf->p24 = 0.0f;  kf->p34 = 0.0f;
  /* cuva se: p22 (theta_h), p23 (theta_h,omega_h), p33 (omega_h) */
}

/* Korekcija mjerenjem kuta vektora struje:  z = theta_vs,  H = [1 0 0 0 0]
 * (pozivatelj treba zatvoriti vrata na niskoj brzini/struji - atan2 je tada sum) */
void KF5_UpdateCV(KF5_data *kf, float theta_vs_meas) {
  float y = wrap_pm_pi(theta_vs_meas - kf->theta_vs);
  kf->last_innovation = y;

  /* Wrap-ambiguity zastita: samo unutar ~0.25 rad od +-pi je wrap predznak nepouzdan
     -> re-seat. Ispod toga je smjer ispravan, pusti filter da sustize u ubrzanju. */
  if (fabsf(y) > 2.9f) {
    kf->theta_vs = theta_vs_meas;
    return;
  }

  float p00 = kf->p00, p01 = kf->p01, p02 = kf->p02, p03 = kf->p03, p04 = kf->p04;
  float                p11 = kf->p11, p12 = kf->p12, p13 = kf->p13, p14 = kf->p14;
  float                              p22 = kf->p22, p23 = kf->p23, p24 = kf->p24;
  float                                             p33 = kf->p33, p34 = kf->p34;
  float                                                            p44 = kf->p44;

  float S  = p00 + kf->r_vs;
  float K0 = p00 / S;  /* theta_vs */
  float K1 = p01 / S;  /* omega_vs  */
  float K2 = p02 / S;  /* theta_h   */
  float K3 = p03 / S;  /* omega_h   */
  float K4 = p04 / S;  /* d_omega   */

  kf->theta_vs = wrap_0_2pi(kf->theta_vs + K0 * y);
  kf->omega_vs += K1 * y;
  kf->theta_h  = wrap_0_2pi(kf->theta_h + K2 * y);
  kf->omega_h  += K3 * y;
  kf->d_omega  += K4 * y;

  /* P = (I - K H) P,  H bira redak/stupac 0  ->  p_ij -= K_i * p_0j */
  kf->p00 = p00 - K0*p00;
  kf->p01 = p01 - K0*p01;
  kf->p02 = p02 - K0*p02;
  kf->p03 = p03 - K0*p03;
  kf->p04 = p04 - K0*p04;
  kf->p11 = p11 - K1*p01;
  kf->p12 = p12 - K1*p02;
  kf->p13 = p13 - K1*p03;
  kf->p14 = p14 - K1*p04;
  kf->p22 = p22 - K2*p02;
  kf->p23 = p23 - K2*p03;
  kf->p24 = p24 - K2*p04;
  kf->p33 = p33 - K3*p03;
  kf->p34 = p34 - K3*p04;
  kf->p44 = p44 - K4*p04;
}

/* Korekcija Hall kutom:  z = theta_h,  H = [0 0 1 0 0]  (samo na bridu) */
void KF5_UpdateHall(KF5_data *kf, float theta_hall, int8_t step) {
  kf->ticks_since_edge = 0;

  /* obrat smjera izmedu dva brida = titranje, ne stvarna rotacija */
  if (step != 0 && step == -kf->last_step) {
    if (kf->dither_count < 255) kf->dither_count++;
  } else {
    kf->dither_count = 0;
  }
  kf->last_step = step;

  if (kf->dither_count >= 2) {            /* zamrzni, ne gradi brzinu */
    kf->omega_vs = 0.0f; kf->omega_h = 0.0f; kf->d_omega = 0.0f;
    kf->theta_h  = theta_hall;
    kf->last_innovation = 0.0f;
    return;
  }

  float p00 = kf->p00, p01 = kf->p01, p02 = kf->p02, p03 = kf->p03, p04 = kf->p04;
  float                p11 = kf->p11, p12 = kf->p12, p13 = kf->p13, p14 = kf->p14;
  float                              p22 = kf->p22, p23 = kf->p23, p24 = kf->p24;
  float                                             p33 = kf->p33, p34 = kf->p34;
  float                                                            p44 = kf->p44;

  float S  = p22 + kf->r_h;
  float K0 = p02 / S;  /* theta_vs */
  float K1 = p12 / S;  /* omega_vs  */
  float K2 = p22 / S;  /* theta_h   */
  float K3 = p23 / S;  /* omega_h   */
  float K4 = p24 / S;  /* d_omega   */

  float y = wrap_pm_pi(theta_hall - kf->theta_h);
  kf->theta_vs = wrap_0_2pi(kf->theta_vs + K0 * y);
  kf->omega_vs += K1 * y;
  kf->theta_h  = wrap_0_2pi(kf->theta_h + K2 * y);
  kf->omega_h  += K3 * y;
  kf->d_omega  += K4 * y;
  kf->last_innovation = y;

  /* P = (I - K H) P,  H bira redak/stupac 2  ->  p_ij -= K_i * p_2j */
  kf->p00 = p00 - K0*p02;
  kf->p01 = p01 - K0*p12;
  kf->p02 = p02 - K0*p22;
  kf->p03 = p03 - K0*p23;
  kf->p04 = p04 - K0*p24;
  kf->p11 = p11 - K1*p12;
  kf->p12 = p12 - K1*p22;
  kf->p13 = p13 - K1*p23;
  kf->p14 = p14 - K1*p24;
  kf->p22 = p22 - K2*p22;
  kf->p23 = p23 - K2*p23;
  kf->p24 = p24 - K2*p24;
  kf->p33 = p33 - K3*p23;
  kf->p34 = p34 - K3*p24;
  kf->p44 = p44 - K4*p24;
}
