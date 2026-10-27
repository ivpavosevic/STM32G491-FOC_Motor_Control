/*
 * kalman.c
 *
 *  Created on: 16 Jun 2026
 *      Author: ivanp
 */
#include "kalman.h"
/* ============ Private variables ============ */

/* ============ Private functions ============ */

/* three estimators here. only KF5 is active - the ADC ISR runs it and uses theta_h to commutate.
 *   HKF  - 2-state Hall-only       [theta, omega]                                   (legacy)
 *   CVKF - 3-state current-vector  [theta, omega, d_omega]                          (legacy)
 *   KF5  - 5-state combined        [theta_vs, omega_vs, theta_h, omega_h, d_omega]  (ACTIVE) */

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

/* Init the 2-state Hall Kalman (theta, omega). Legacy - not wired into the ISR now. */
void HallKF_Init(HallKF_data *kf, float theta_0) {
  kf->theta = theta_0;
  kf->theta_init = theta_0;
  kf->omega = 0.0f;
  kf->p00 = 1.0f; // 0.1
  kf->p01 = 0.0f; // 0.1
  kf->p11 = 3.0f; // 10.0
  kf->ts = 0;

  // Process noise (start conservative for smooth omega)
  kf->q_theta = 0.0001f; // working: 0.0001
  kf->q_omega = 0.01f; // working_ 0.005

  kf->r_theta = 0.1f; // working: 0.1
  kf->r_omega = 10.0f;

  kf->K_theta = 0.0f;
  kf->K_omega = 0.0f;

  kf->t_standstill = HKF_STANDSTILL_INIT;
  kf->ticks_since_edge = 0;
  kf->last_innovation = 0.0f;
  kf->inited = 0;
  kf->last_theta_meas = 0.0f;
  kf->second_last_theta_meas = 0.0f;
  kf->last_step = 0;
  kf->dither_count = 0;
}

/* Init the 3-state current-vector Kalman (theta, omega, d_omega=accel). Legacy - not used now. */
void CurrVectorKF_Init(CurrVectorKF_data *cvkf, float theta_0) {
  cvkf->theta   = theta_0;
  cvkf->omega   = 0.0f;
  cvkf->d_omega = 0.0f;   // acceleration

  /* P: angle known-ish, speed unknown, acceleration very unknown; no cross-corr yet */
  cvkf->p00 = 0.1f;  cvkf->p01 = 0.0f;  cvkf->p02 = 0.0f;
                     cvkf->p11 = 10.0f; cvkf->p12 = 0.0f;
                                        cvkf->p22 = 100.0f;
  cvkf->ts  = 0.0f;

  /* Q (diagonal) --- TUNE --- */
  cvkf->q_theta   = 1e-4f;
  cvkf->q_omega   = 0.02f;
  cvkf->q_d_omega = 0.1f;    // accel must build FAST to track 40V ramp w/o theta lag->wrap flip

  // atan2 angle variance
  cvkf->r_atan2 = 0.25f;

  cvkf->K_theta   = 0.0f;
  cvkf->K_omega   = 0.0f;
  cvkf->K_d_omega = 0.0f;

  cvkf->last_innovation = 0.0f;
}

/* Init the ACTIVE 5-state filter. State = [theta_vs, omega_vs, theta_h, omega_h, d_omega];
 * theta_h is the angle handed to FOC. Run once per motor at startup. */
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
  kf->q_theta_h  = 1e-4f;
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


// Prediction step every 40us, part of ADC interrupt
void HKF_Predict(HallKF_data *hkf, float cvkf_omega, float cvkf_d_omega, float dt) {
// -------- predict for rotor Kalman -------------------
	(void)cvkf_omega;   // connection uses only the CVKF acceleration (cvkf_d_omega)
	hkf->ts = dt;

    hkf->ticks_since_edge++;
    if (hkf->ticks_since_edge > hkf->t_standstill) {
        // No Hall edge for too long
        hkf->omega = 0.0f;
        hkf->p01 = 0.0f;
        hkf->p11 = P11_AT_REST;
    } else {
    	float Ts = hkf->ts;
    	float boost = 0.0f;
    	if (fabsf(cvkf_d_omega) > BOOST_DEADBAND) {
    		float t_since = (float)hkf->ticks_since_edge * Ts;   // time since last edge
    		if (t_since > T_SINCE_MAX) t_since = T_SINCE_MAX;
    		boost = BOOST_GAIN * cvkf_d_omega * t_since;
    		float lim = BOOST_CLAMP * fabsf(hkf->omega);
    		if (boost >  lim) boost =  lim;
    		if (boost < -lim) boost = -lim;
    	}
    	float omega_eff = hkf->omega + boost;

    	// x^- = A * x  (angle advances at the extrapolated speed)
        hkf->theta = wrap_0_2pi(hkf->theta + Ts * omega_eff);

    	// P^- = A*P*A^T + Q  (constant-velocity model on [theta, omega])
    	float p00 = hkf->p00, p01 = hkf->p01, p11 = hkf->p11;
    	hkf->p00 = p00 + 2.0f*Ts*p01 + (Ts*Ts)*p11 + hkf->q_theta;
    	hkf->p01 = p01 + Ts*p11;
    	hkf->p11 = p11 + hkf->q_omega;
    }




}

// Update step done when new Hall information is present
void HKF_Update(HallKF_data *hkf, float theta_meas, int8_t step) {
	hkf->ticks_since_edge = 0; // new edge arrived

    // Direction reversal between consecutive edges = not real rotation
    if (step != 0 && step == -hkf->last_step) {
        if (hkf->dither_count < 255) hkf->dither_count++;
    } else {
        hkf->dither_count = 0;
    }
    hkf->last_step = step;

    if (hkf->dither_count >= 2) {
        // Bouncing between two adjacent sectors → freeze, don't build speed
        hkf->omega = 0.0f;
        hkf->theta = theta_meas;   // just track the measurement
        hkf->last_innovation = 0.0f;
        return;                                 // skip the gain update entirely
    }


	// update Kalman gain K = (P*H^T)  / (H * P * H^T + R) => K(2x1 matrix)
	hkf->K_theta = hkf->p00 / (hkf->p00 + hkf->r_theta);
	hkf->K_omega = hkf->p01 / (hkf->p00 + hkf->r_theta);

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

//	float S   = hkf->p11 + hkf->r_omega;             // note: p11, not p00
//	float K_t = hkf->p01 / S;
//	float K_w = hkf->p11 / S;
//	float y_w = omega_meas - hkf->omega;
//	hkf->theta = wrap_0_2pi(hkf->theta + K_t * y_w);
//	hkf->omega = hkf->omega + K_w * y_w;
//	hkf->p00  -= K_t * hkf->p01;                     // old p01
//	hkf->p01  *= (1 - K_w);
//	hkf->p11  *= (1 - K_w);


	hkf->last_innovation = innovation;
//	if(theta_meas == hkf->second_last_theta_meas){
//
//		if(hkf->ticks_since_edge++ >= 100 ){
//			hkf->omega = 0.0f;
//		}
//	}
//	hkf->second_last_theta_meas = hkf->last_theta_meas;
//	hkf->last_theta_meas = theta_meas;


}

// Prediction step for CVKF: 3-state constant-acceleration model
//   x = [theta, omega, d_omega],  A = [[1,Ts,0],[0,1,Ts],[0,0,1]]
//   theta advances at omega, omega advances at d_omega (=acceleration).
void CVKF_Predict(CurrVectorKF_data *cvkf, float omega_hkf, float dt) {
	(void)omega_hkf;   // not used in the constant-acceleration model
	cvkf->ts = dt;
	float Ts = dt;

	// x^- = A x
	cvkf->theta = wrap_0_2pi(cvkf->theta + Ts * cvkf->omega);
	cvkf->omega = cvkf->omega + Ts * cvkf->d_omega;
	// d_omega (acceleration) unchanged (random walk via Q)

	// P^- = A P A^T + Q  (3x3, derived element-by-element; snapshot old values)
	float p00 = cvkf->p00, p01 = cvkf->p01, p02 = cvkf->p02;
	float                  p11 = cvkf->p11, p12 = cvkf->p12;
	float                                   p22 = cvkf->p22;

	cvkf->p00 = p00 + 2.0f*Ts*p01 + Ts*Ts*p11 + cvkf->q_theta;
	cvkf->p01 = p01 + Ts*(p11 + p02) + Ts*Ts*p12;
	cvkf->p02 = p02 + Ts*p12;
	cvkf->p11 = p11 + 2.0f*Ts*p12 + Ts*Ts*p22 + cvkf->q_omega;
	cvkf->p12 = p12 + Ts*p22;
	cvkf->p22 = p22 + cvkf->q_d_omega;
}


// Update from atan2 angle measurement.  z = theta,  H = [1 0 0]
void CVKF_Update(CurrVectorKF_data *cvkf, float theta_atan2) {
	float y = wrap_pm_pi(theta_atan2 - cvkf->theta);
	cvkf->last_innovation = y;

	// Wrap-ambiguity guard: ONLY within ~0.25 rad of +-pi is the wrapped sign
	// unreliable (a full lap +eps reads as -pi+eps). There we re-seat instead of
	// correcting the wrong way. Below CVKF_INNOV_MAX the direction is valid, so we
	// let the filter use it to catch up during hard acceleration (do NOT reject).
	if (fabsf(y) > CVKF_INNOV_MAX) {
		cvkf->theta = theta_atan2;   // snap angle; keep omega/d_omega this cycle
		return;
	}

	// snapshot pre-update P (needed so the covariance update is consistent)
	float p00 = cvkf->p00, p01 = cvkf->p01, p02 = cvkf->p02;
	float                  p11 = cvkf->p11, p12 = cvkf->p12;
	float                                   p22 = cvkf->p22;

	float S = p00 + cvkf->r_atan2;
	cvkf->K_theta   = p00 / S;
	cvkf->K_omega   = p01 / S;
	cvkf->K_d_omega = p02 / S;

	cvkf->theta   = wrap_0_2pi(cvkf->theta + cvkf->K_theta * y);
	cvkf->omega   += cvkf->K_omega   * y;
	cvkf->d_omega += cvkf->K_d_omega * y;

	// P = (I - K H) P,  H picks row/col 0  ->  p_ij -= K_i * p_0j  (use snapshots)
	cvkf->p00 = p00 - cvkf->K_theta   * p00;
	cvkf->p01 = p01 - cvkf->K_theta   * p01;
	cvkf->p02 = p02 - cvkf->K_theta   * p02;
	cvkf->p11 = p11 - cvkf->K_omega   * p01;
	cvkf->p12 = p12 - cvkf->K_omega   * p02;
	cvkf->p22 = p22 - cvkf->K_d_omega * p02;
}

// Idle hold: call every cycle while the CV measurement is gated off (low speed).
// Predict-only running inflates P unbounded (p22 climbs at q_d_omega*25kHz); on
// re-engagement that gives gains >> 1 that slam the states and collapse the filter.
// Re-seating states + P to init each idle cycle keeps re-engagement smooth. theta is
// seeded to the live measurement so the first active innovation is ~0.
void CVKF_Idle(CurrVectorKF_data *cvkf, float theta_seed, float omega_seed) {
	cvkf->theta   = theta_seed;
	cvkf->omega   = omega_seed;
	cvkf->d_omega = 0.0f;
	cvkf->p00 = 0.1f;  cvkf->p01 = 0.0f;  cvkf->p02 = 0.0f;
	                   cvkf->p11 = 10.0f; cvkf->p12 = 0.0f;
	                                      cvkf->p22 = 10.0f;
	cvkf->last_innovation = 0.0f;
}

/* KF5 predict - runs every ADC cycle: advance both angles at their speeds and grow P.
 * No Hall edge for too long -> treat as standstill (zero the speeds, reset P).
 * Input: dt (cycle time) */
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

/* Re-seat the current-vector branch to init while the CV measurement is gated off,
 * so its covariance can't blow up at low speed. NOTE: not called in the ISR currently. */
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

/* KF5 correction from the current-vector angle - runs every cycle.
 * Input: theta_vs_meas (the Im/Re angle). Measures state 0, H = [1 0 0 0 0]. */
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

/* KF5 correction from a Hall edge (only on a new edge) - the absolute anchor.
 * Input: theta_hall, step. Measures state 2 (theta_h), H = [0 0 1 0 0]. */
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


