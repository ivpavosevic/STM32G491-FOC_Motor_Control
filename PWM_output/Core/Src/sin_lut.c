#include <math.h>
#include "sin_lut.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

float sin_LUT[SIN_LUT_SIZE];
float sin_LUT_test[SIN_LUT_SIZE];

int sinLUT_Init(void)
{
	for (int i = 0; i < SIN_LUT_SIZE; i++){
		float angle = (2.0f * M_PI * (float)i)/ (float)SIN_LUT_SIZE;
		sin_LUT[i] = sin(angle);
		sin_LUT_test[i] = 0.5f + 0.5f * sin_LUT[i];
	}

	return SUCCESS;
}

float sinLUT_Get(uint16_t index)
{
	return sin_LUT[index % SIN_LUT_SIZE];
}
