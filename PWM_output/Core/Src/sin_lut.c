#include <math.h>
#include "sin_lut.h"
#include "main.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define SIN_LUT_SIZE 512

float sin_LUT[SIN_LUT_SIZE];

void sinLUT_Init(void)
{
	for (int i = 0; i < SIN_LUT_SIZE; i++){
		float angle = (2 * M_PI * (float)i)/ (float)SIN_LUT_SIZE;
		sin_LUT[i] = sinf(angle);
	}
}

float sinLUT_Get(int index)
{
	return sin_LUT[index];
}
