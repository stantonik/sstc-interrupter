/*
 * Copyright (C) 2025 Stanley Arnaud <stantonik@stantonik-mba.local>
 *
 * Distributed under terms of the MIT license.
 */

/**
 * @file pwm.c
 * @brief
 *
 * @author Stanley Arnaud
 * @date 09/26/2025
 * @version 0
 */

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------
#include "pwm.h"
#include "audio.h"
#include "driver/ledc.h"
#include "driver/rmt.h"
#include "rom/gpio.h"
#include "sdkconfig.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"

// -----------------------------------------------------------------------------
// Macros and Constants
// -----------------------------------------------------------------------------
#define PIN_OUTPUT CONFIG_INTERRUPT_PIN_OUTPUT

#define RMT_CHANNEL RMT_CHANNEL_0
#define RMT_CLK_DIV 80 // 80 MHz / 80 = 1 MHz → 1 tick = 1 us
#define RMT_TICK_US (80 / RMT_CLK_DIV)

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY (30e3)

#define TAG "pwm"

// -----------------------------------------------------------------------------
// Static Variables
// -----------------------------------------------------------------------------
static pwm_mode_t mode = PWM_MANUAL;
static QueueHandle_t lpwm_task_queue = NULL;
static TaskHandle_t lpwm_task_handle = NULL;
static bool armed = false;

// -----------------------------------------------------------------------------
// Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Static Function Definitions
// -----------------------------------------------------------------------------
static void lpwm_task(void *pvParams)
{
    uint32_t period_tick = 0;
    uint32_t pulse_width_tick = 0;
    uint32_t pwm_data[2];

    while (1)
    {
        // Try to receive new PWM data (non-blocking)
        if (xQueueReceive(lpwm_task_queue, &pwm_data, 0) == pdPASS)
        {
            period_tick = pwm_data[0];
            pulse_width_tick = pwm_data[1];
        }

        // If period or pulse_width is zero, block until valid value arrives
        while (period_tick == 0 || pulse_width_tick == 0)
        {
            // Ensure output pin is low
            WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1ULL << PIN_OUTPUT));

            // Wait for new PWM data
            if (xQueueReceive(lpwm_task_queue, &pwm_data, portMAX_DELAY) ==
                pdPASS)
            {
                period_tick = pwm_data[0];
                pulse_width_tick = pwm_data[1];
            }
        }

        rmt_item32_t item;
        item.level0 = 1;
        item.duration0 = pulse_width_tick; // high time
        item.level1 = 0;
        item.duration1 = pulse_width_tick; // low time

        rmt_write_items(RMT_CHANNEL, &item, 1, false);
        vTaskDelay(pdMS_TO_TICKS((period_tick * RMT_TICK_US) / 1000));
    }
}

static void pwm_ledc_set_duty(uint8_t duty)
{
    // if (duty > 512) duty = 512;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

// -----------------------------------------------------------------------------
// Function Definitions
// -----------------------------------------------------------------------------
void pwm_init(void)
{
    lpwm_task_queue = xQueueCreate(2, sizeof(uint32_t[2]));

    ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_MODE,
                                          .channel = LEDC_CHANNEL,
                                          .timer_sel = LEDC_TIMER,
                                          .intr_type = LEDC_INTR_DISABLE,
                                          .gpio_num = PIN_OUTPUT,
                                          .duty = 0,
                                          .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_CHANNEL,
        .gpio_num = PIN_OUTPUT,
        .clk_div = RMT_CLK_DIV,
        .mem_block_num = 1,
        .tx_config.loop_en = true,
        .tx_config.carrier_en = false,
    };
    rmt_config(&config);
    rmt_driver_install(config.channel, 0, 0);

    xTaskCreate(lpwm_task, "low_pwm_task", 1024, NULL, 2, &lpwm_task_handle);

    audio_init();
    audio_set_pwm_duty_update_cb(pwm_ledc_set_duty);
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_MODE,
                                      .duty_resolution = LEDC_DUTY_RES,
                                      .timer_num = LEDC_TIMER,
                                      .freq_hz = LEDC_FREQUENCY,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    pwm_disarm();
}

void pwm_arm(void)
{
    armed = true;
    if (mode == PWM_MANUAL)
    {
        gpio_matrix_out(PIN_OUTPUT, RMT_SIG_OUT0_IDX, 0, 0);
    }
    else if (mode == PWM_AUDIO)
    {
        gpio_matrix_out(PIN_OUTPUT, LEDC_LS_SIG_OUT0_IDX, 0, 0);
    }
}

void pwm_disarm(void)
{
    armed = false;
    gpio_matrix_out(PIN_OUTPUT, SIG_GPIO_OUT_IDX, 0, 0);
}

void pwm_set_mode(pwm_mode_t pwm_mode)
{
    if (pwm_mode == PWM_AUDIO)
    {
        if (mode == PWM_AUDIO) return;

        // Stop MANUAL
        vTaskSuspend(lpwm_task_handle);
        rmt_tx_stop(RMT_CHANNEL);
        WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1ULL << PIN_OUTPUT));

        // Start AUDIO
        if (armed) gpio_matrix_out(PIN_OUTPUT, LEDC_LS_SIG_OUT0_IDX, 0, 0);
        audio_listen();
    }
    else if (pwm_mode == PWM_MANUAL)
    {
        if (mode == PWM_MANUAL) return;

        // Stop AUDIO
        audio_stop();
        ledc_stop(LEDC_MODE, LEDC_CHANNEL_0, 0);
        WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1ULL << PIN_OUTPUT));

        // Start MANUAL
        if (armed) gpio_matrix_out(PIN_OUTPUT, RMT_SIG_OUT0_IDX, 0, 0);
        vTaskResume(lpwm_task_handle);
    }

    mode = pwm_mode;
}

pwm_mode_t pwm_get_mode(void) { return mode; }

void pwm_manual_update(uint16_t freq_hz, uint16_t pulse_width_us)
{
    if (mode != PWM_MANUAL) return;

    uint32_t pwm_data[2] = {0};
    if (freq_hz == 0 || pulse_width_us == 0)
    {
        // Stop output
        rmt_tx_stop(RMT_CHANNEL);
        xQueueSend(lpwm_task_queue, pwm_data, 0);
        WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1ULL << PIN_OUTPUT));
        return;
    }

    uint32_t period_tick = 1000000UL / (freq_hz * RMT_TICK_US);
    uint32_t pulse_width_tick = pulse_width_us / RMT_TICK_US;
    if (pulse_width_tick > period_tick / 2) pulse_width_tick = period_tick / 2;

    if (period_tick > 32767) // rmt_item32_t.duration0/1 overflow
    {
        rmt_set_tx_loop_mode(RMT_CHANNEL, false);
        pwm_data[0] = period_tick;
        pwm_data[1] = pulse_width_tick;

        xQueueSend(lpwm_task_queue, pwm_data, 0);
    }
    else
    {
        xQueueSend(lpwm_task_queue, pwm_data, 0);
        rmt_set_tx_loop_mode(RMT_CHANNEL, true);

        rmt_item32_t item;
        item.level0 = 1;
        item.duration0 = pulse_width_tick; // high time
        item.level1 = 0;
        item.duration1 = period_tick - pulse_width_tick; // low time

        rmt_write_items(RMT_CHANNEL, &item, 1, false);
    }
}
