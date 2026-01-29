/*
 * cordic_sin.h
 *
 * CORDIC-based sine/cosine calculation for motor control
 * Supports variable angle increment for speed control
 *
 *  Created on: Jan 29, 2026
 */

#ifndef INC_CORDIC_SIN_H_
#define INC_CORDIC_SIN_H_

#include "main.h"

/* Return codes */
#define CORDIC_SIN_OK 0
#define CORDIC_SIN_ERROR -1

/**
 * @brief Initialize CORDIC sine module
 * @param hcordic_ptr Pointer to CORDIC handle (from CubeMX)
 * @param pwm_freq_hz PWM frequency in Hz (for frequency calculations)
 * @retval CORDIC_SIN_OK on success
 */
int CORDIC_Sin_Init(CORDIC_HandleTypeDef *hcordic_ptr, float pwm_freq_hz);

/**
 * @brief Set electrical rotation frequency
 * @param freq_hz Desired electrical frequency in Hz
 *        Example: 50 Hz = motor rotates at 50/pole_pairs RPS
 */
void CORDIC_Sin_SetFrequency(float freq_hz);

/**
 * @brief Get current electrical frequency
 * @retval Current frequency in Hz
 */
float CORDIC_Sin_GetFrequency(void);

/**
 * @brief Reset angle to zero
 */
void CORDIC_Sin_ResetAngle(void);

/**
 * @brief Get 3-phase sine values (0°, 120°, 240°) and advance angle
 * @param sin_a Pointer to Phase A sine value (-1.0 to 1.0)
 * @param sin_b Pointer to Phase B sine value (120° offset)
 * @param sin_c Pointer to Phase C sine value (240° offset)
 * @retval CORDIC_SIN_OK on success
 *
 * @note This function automatically advances the angle by delta after reading
 */
int CORDIC_Sin_Get3Phase(float *sin_a, float *sin_b, float *sin_c);

/**
 * @brief Get single sine value at current angle and advance
 * @param sin_out Pointer to store sine value (-1.0 to 1.0)
 * @param cos_out Pointer to store cosine value (can be NULL)
 * @retval CORDIC_SIN_OK on success
 */
int CORDIC_Sin_Get(float *sin_out, float *cos_out);

/**
 * @brief Get sine value with phase offset (does NOT advance angle)
 * @param phase_offset_deg Phase offset in degrees (e.g., 120.0f)
 * @param sin_out Pointer to store sine value
 * @retval CORDIC_SIN_OK on success
 */
int CORDIC_Sin_GetWithOffset(float phase_offset_deg, float *sin_out);

/**
 * @brief Manually advance angle (if not using auto-advance in Get functions)
 */
void CORDIC_Sin_Advance(void);

#endif /* INC_CORDIC_SIN_H_ */
