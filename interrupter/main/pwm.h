/*
 * Copyright (C) 2025 Stanley Arnaud <stantonik@stantonik-mba.local>
 *
 * Distributed under terms of the MIT license.
 */

/**
 * @file pwm.h
 * @brief 
 *
 * 
 *
 * @author Stanley Arnaud 
 * @date 09/26/2025
 * @version 0
 */

#ifndef PWM_H
#define PWM_H

// clang-format off
#include <stdint.h>
#ifdef __cplusplus
extern "C" 
{
#endif

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Macros and Constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Type Definitions
// -----------------------------------------------------------------------------
typedef enum {
    PWM_MANUAL,
    PWM_AUDIO
} pwm_mode_t;

// -----------------------------------------------------------------------------
// Inline Function Definitions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Function Declarations
// -----------------------------------------------------------------------------
void pwm_init(void);
void pwm_set_mode(pwm_mode_t mode);
pwm_mode_t pwm_get_mode(void);
void pwm_manual_update(uint16_t freq_hz, uint16_t pulse_width_us);
void pwm_arm(void);
void pwm_disarm(void);


#ifdef __cplusplus
}
#endif
// clang-format on

#endif /* !PWM_H */

