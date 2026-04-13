/*
 * ang_est_vectors.c
 *
 *  Created on: Mar 19, 2026
 *      Author: ivanp
 */
#include "ang_est_vectors.h"

float AngEst_Calculate(float Ialfa_now, float Ibeta_now, float Ialfa_prev, float Ibeta_prev){
	// Z[n] = Ialfa[n] + Ibeta[n]
	// prev: Z1 = a + j*b ---> [n-1]
	// now: Z2 = c + j*d ---> [n]
	// To get angle between ---> ATAN2(bc - ad, ac +bd) -> (using CORDIC)

	return (Ibeta_prev * Ialfa_now - Ialfa_prev * Ibeta_now)
			/ ( Ialfa_prev * Ialfa_now + Ibeta_prev * Ibeta_now);
}

