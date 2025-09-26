/*
 * Copyright (C) 2025 Stanley Arnaud <stantonik@stantonik-mba.local>
 *
 * Distributed under terms of the MIT license.
 */

#include "button_gpio.h"
#include "esp_log.h"
#include "iot_button.h"
#include "menu.h"
#include "pwm.h"
#include <stdbool.h>
#include <stdint.h>

#define PIN_TRIGGER CONFIG_INTERRUPT_PIN_TRIGGER
#define PIN_JACK_SW CONFIG_INTERRUPT_PIN_JACK_SW

#define TAG "interrupter"

button_handle_t trigger_btn = NULL;
button_handle_t jack_btn = NULL;

static void button_down_cb(void *arg, void *usr_data)
{
    button_handle_t btn = (button_handle_t)arg;

    if (btn == trigger_btn)
    {
        ESP_LOGI(TAG, "Trigger button pressed → Armed");
        pwm_arm();
    }
    else if (btn == jack_btn)
    {
        ESP_LOGI(TAG, "Jack button pressed → Audio mode");
        pwm_set_mode(PWM_AUDIO);
    }
}

static void button_up_cb(void *arg, void *usr_data)
{
    button_handle_t btn = (button_handle_t)arg;

    if (btn == trigger_btn)
    {
        ESP_LOGI(TAG, "Trigger button released → Disarmed");
        pwm_disarm();
    }
    else if (btn == jack_btn)
    {
        ESP_LOGI(TAG, "Jack button released → Manual mode");
        pwm_set_mode(PWM_MANUAL);
    }
}

void app_main(void)
{
    menu_init();
    pwm_init();
    pwm_set_mode(PWM_MANUAL);

    button_config_t btn_cfg = {0};

    button_gpio_config_t trigger_gpio_cfg = {
        .gpio_num = PIN_TRIGGER,
        .active_level = 0,
    };
    iot_button_new_gpio_device(&btn_cfg, &trigger_gpio_cfg, &trigger_btn);

    button_gpio_config_t jack_gpio_cfg = {
        .gpio_num = PIN_JACK_SW,
        .active_level = 0,
    };
    iot_button_new_gpio_device(&btn_cfg, &jack_gpio_cfg, &jack_btn);

    iot_button_register_cb(trigger_btn, BUTTON_PRESS_DOWN, NULL, button_down_cb,
                           (void *)trigger_btn);
    iot_button_register_cb(trigger_btn, BUTTON_PRESS_UP, NULL, button_up_cb,
                           (void *)trigger_btn);

    iot_button_register_cb(jack_btn, BUTTON_PRESS_DOWN, NULL, button_down_cb,
                           (void *)jack_btn);
    iot_button_register_cb(jack_btn, BUTTON_PRESS_UP, NULL, button_up_cb,
                           (void *)jack_btn);
}
