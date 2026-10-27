/*
 * pi_reg.h
 *
 *  Created on: 16 Jun 2026
 *      Author: ivanp
 */

#ifndef INC_PI_REG_H_
#define INC_PI_REG_H_

#include "main.h"

void PI_Init_q(PI_reg_t *pi_iq);

void PI_Init_d(PI_reg_t *pi_id);

void PI_Init_omega(PI_reg_t *pi_omega);

float Control_PI_reg(PI_reg_t *pi_x, float err, float dt_s);

#endif /* INC_PI_REG_H_ */
