/*
 * Copyright (C) 2025 Stanley Arnaud <stantonik@stantonik-mba.local>
 *
 * Distributed under terms of the MIT license.
 */

/**
 * @file audio.c
 * @brief
 *
 * @author Stanley Arnaud
 * @date 09/26/2025
 * @version 0
 */

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------
#include "audio.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"

// -----------------------------------------------------------------------------
// Macros and Constants
// -----------------------------------------------------------------------------
#define ADC_UNIT ADC_UNIT_1
#define ADC_CHANNEL ADC_CHANNEL_0
#define SAMPLE_RATE_HZ 16000
#define CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define ADC_BITWIDTH ADC_BITWIDTH_12

#define MIDPOINT 2110

// -----------------------------------------------------------------------------
// Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Static Function Definitions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Function Definitions
// -----------------------------------------------------------------------------
static adc_continuous_handle_t adc_handle = NULL;
static audio_state_t state = AUDIO_IDLE;
static uint8_t volume = 0;

typedef struct
{
    size_t size;
} adc_evt_t;

static void (*pwm_duty_cb)(uint8_t duty);

// --- ISR: only notify task ---
static bool IRAM_ATTR adc_conv_done_cb(adc_continuous_handle_t handle,
                                       const adc_continuous_evt_data_t *edata,
                                       void *user_data)
{
    adc_digi_output_data_t *p =
        (adc_digi_output_data_t *)(edata->conv_frame_buffer);
    uint16_t raw = p->type2.data;

    int mapped_val = raw * 255 / 4095;
    if (mapped_val < 0) mapped_val = 0;
    if (mapped_val > 255) mapped_val = 255;

    if (pwm_duty_cb) pwm_duty_cb(mapped_val);

    return true;
}

void audio_init(void)
{
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = 2048,
        .conv_frame_size = sizeof(adc_digi_output_data_t),
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = ADC_CHANNEL,
        .unit = ADC_UNIT,
        .bit_width = ADC_BITWIDTH,
    };

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_RATE_HZ,
        .conv_mode = CONV_MODE,
        .format = OUTPUT_TYPE,
        .pattern_num = 1,
        .adc_pattern = &adc_pattern,
    };
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = adc_conv_done_cb,
    };
    adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL);
}

void audio_listen(void)
{
    if (state == AUDIO_LISTENING) return;
    state = AUDIO_LISTENING;
    adc_continuous_start(adc_handle);
}

void audio_stop(void)
{
    if (state == AUDIO_IDLE) return;
    state = AUDIO_IDLE;
    adc_continuous_stop(adc_handle);
}

void audio_set_pwm_duty_update_cb(void (*cb)(uint8_t duty))
{
    pwm_duty_cb = cb;
}

void audio_set_volume(uint8_t vol) { volume = vol; }
