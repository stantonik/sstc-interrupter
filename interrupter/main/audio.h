/*
 * Copyright (C) 2025 Stanley Arnaud <stantonik@stantonik-mba.local>
 *
 * Distributed under terms of the MIT license.
 */

/**
 * @file audio.h
 * @brief
 *
 *
 *
 * @author Stanley Arnaud
 * @date 09/26/2025
 * @version 0
 */

#ifndef AUDIO_H
#define AUDIO_H

// clang-format off
#ifdef __cplusplus
extern "C" 
{
#endif

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------
#include <stdint.h>

// -----------------------------------------------------------------------------
// Macros and Constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Type Definitions
// -----------------------------------------------------------------------------
typedef enum 
{
    AUDIO_LISTENING,
    AUDIO_IDLE
} audio_state_t;

// -----------------------------------------------------------------------------
// Inline Function Definitions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Function Declarations
// -----------------------------------------------------------------------------
void audio_init(void);
void audio_listen(void);
void audio_stop(void);
audio_state_t audio_get_state(void);
void audio_set_pwm_duty_update_cb(void (*cb)(uint8_t duty));
void audio_set_volume(uint8_t saturation_factor);

#ifdef __cplusplus
}
#endif
// clang-format on

#endif /* !AUDIO_H */
