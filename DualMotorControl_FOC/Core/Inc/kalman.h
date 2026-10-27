/*
 * kalman.h
 *
 *  Created on: 16 Jun 2026
 *      Author: ivanp
 */

#ifndef INC_KALMAN_H_
#define INC_KALMAN_H_

#include "main.h"

#define HKF_STANDSTILL_INIT 15000.0f // if no moving for 1s

#define P11_AT_REST  10.0f
#define P_MIN 1e-6f

#define K5_OMEGA_GATE 1.0f

#define CV_OMEGA_GATE 20.0f      // rad/s elec: below this the measurement is noise -> no boost
#define CVKF_INNOV_MAX  2.9f     // rad: re-seat ONLY within ~0.25 rad of pi (wrap sign ambiguous);
                                 // below this the innovation direction is valid -> let it track
/* ---- containment of the CVKF acceleration boost (bench-derived, step-1 capture) ----
 * measured: real accel ~330 rad/s^2, fast noise ~±75, once-per-mech-rev spikes ~-300 */
#define BOOST_GAIN      0.3f     // apply 30% of estimated accel; raise once stable
#define BOOST_DEADBAND  150.0f   // rad/s^2: below this = noise, no boost
#define BOOST_CLAMP     0.10f    // max |boost| as fraction of |omega| (~6 deg/interval)
#define T_SINCE_MAX     0.005f   // s: cap extrapolation time (low speed = weak atan2)


void HallKF_Init(HallKF_data *kf, float theta_0);

void CurrVectorKF_Init(CurrVectorKF_data *cvkf, float theta_0);

void HKF_Predict(HallKF_data *hkf, float cvkf_omega, float cvkf_d_omega, float dt);
void CVKF_Predict(CurrVectorKF_data *cvkf, float omega_hkf, float dt);

void HKF_Update(HallKF_data *hkf, float theta_meas, int8_t step);
void CVKF_Update(CurrVectorKF_data *cvkf, float theta_atan2);

void CVKF_Idle(CurrVectorKF_data *cvkf, float theta_seed, float omega_seed); // hold while gated off

void KF5_Init(KF5_data *kf, float theta_0);
void KF5_Predict(KF5_data *kf, float dt);
void KF5_UpdateCV(KF5_data *kf, float theta_vs_meas);             /* atan2 vektor struje */
void KF5_UpdateHall(KF5_data *kf, float theta_hall, int8_t step); /* Hall brid */
void KF5_IdleCV(KF5_data *kf, float theta_vs_seed);              /* hold CV grane dok je zatvoreno */


#endif /* INC_KALMAN_H_ */
