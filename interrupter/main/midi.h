/*
 * Copyright (C) 2025 Stanley Arnaud <stantonik@stantonik-mba.local>
 *
 * Distributed under terms of the MIT license.
 */

/**
 * @file midi.h
 * @brief 
 *
 * 
 *
 * @author Stanley Arnaud 
 * @date 10/01/2025
 * @version 0
 */

#ifndef MIDI_H
#define MIDI_H

// clang-format off
#ifdef __cplusplus
extern "C" 
{
#endif

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// Macros and Constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Type Definitions
// -----------------------------------------------------------------------------
typedef struct
{
    bool state;
    uint8_t note;
    uint8_t velocity;
} midi_message_t;

typedef enum
{
    MIDI_EVENT_DEV_CONNECTED,
    MIDI_EVENT_MSG_RECEIVED,
    MIDI_EVENT_DEV_DISCONNECTED,
} midi_event_t;

typedef struct
{
    midi_event_t type;
    midi_message_t msg;
    char *dev_name;
} midi_event_data_t;

// -----------------------------------------------------------------------------
// Inline Function Definitions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Function Declarations
// -----------------------------------------------------------------------------
void midi_init(void);

void midi_set_event_callback(void (*cb)(midi_event_data_t *event));
const char *midi_get_device_name(void);
bool midi_is_connected();

void midi_free(void);

#ifdef __cplusplus
}
#endif
// clang-format on

#endif /* !MIDI_H */

