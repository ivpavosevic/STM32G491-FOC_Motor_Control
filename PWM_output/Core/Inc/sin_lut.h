/*
 * sin_lut.h
 *
 *  Created on: Dec 11, 2025
 *      Author: ivanp
 */

#ifndef INC_SIN_LUT_H_
#define INC_SIN_LUT_H_

#include "main.h"

#define SIN_LUT_SIZE 300
#define SUCCESS 0

int sinLUT_Init(void);
float sinLUT_Get(uint16_t index);

#endif /* INC_SIN_LUT_H_ */
