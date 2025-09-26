/*
 * Copyright (C) 2025 Stanley Arnaud <stantonik@stantonik-mba.local>
 *
 * Distributed under terms of the MIT license.
 */

/**
 * @file menu.c
 * @brief
 *
 * @author Stanley Arnaud
 * @date 09/26/2025
 * @version 0
 */

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------
#include "menu.h"
#include "audio.h"
#include "driver/i2c.h"
#include "encoder.h"
#include "esp_err.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "pwm.h"
#include "sdkconfig.h"
#include "ui/ui.h"
#include <stdint.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Macros and Constants
// -----------------------------------------------------------------------------
#define TAG "menu"

#define PIN_RE_A CONFIG_INTERRUPT_PIN_RE_A
#define PIN_RE_B CONFIG_INTERRUPT_PIN_RE_B
#define PIN_RE_SWT CONFIG_INTERRUPT_PIN_RE_SWT

#define PIN_SDA CONFIG_INTERRUPT_PIN_SDA
#define PIN_SCL CONFIG_INTERRUPT_PIN_SCL

#define I2C_HOST 0

#define LCD_PIXEL_CLOCK_HZ (400 * 1000)
#define I2C_HW_ADDR 0x3C

#define LCD_H_RES 128
#define LCD_V_RES 64
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

// -----------------------------------------------------------------------------
// Static Variables
// -----------------------------------------------------------------------------
typedef struct
{
    char name[4];
    int16_t value;
    int16_t min_value;
    int16_t max_value;
    uint16_t steps[2];
    lv_obj_t *val_txt;
    lv_obj_t *name_txt;
} range_t;

static range_t pd_range = {
    .name = "PD", .min_value = 0, .max_value = 100, .steps = {1, 10}};

static range_t prf_range = {
    .name = "PRF", .min_value = 0, .max_value = 20e3, .steps = {1, 100}};

static range_t *ranges[2] = {
    &pd_range,
    &prf_range,
};

static uint8_t range_count = 2;

static QueueHandle_t re_event_queue;
static rotary_encoder_t re;
static range_t *sel_range = NULL;
static uint8_t sel_range_ind = 0;
static uint8_t step_ind = 0;
static bool editing = false;

static bool trigger_state = false;
static bool jack_plugged = false;

static lv_disp_t *disp = NULL;

// -----------------------------------------------------------------------------
// Static Function Definitions
// -----------------------------------------------------------------------------
static void menu_task(void *pvParam)
{
    while (1)
    {
        // Rotary encoder
        rotary_encoder_event_t e;
        xQueueReceive(re_event_queue, &e, portMAX_DELAY);

        switch (e.type)
        {
        case RE_ET_BTN_PRESSED:
            ESP_LOGI(TAG, "Button pressed");
            break;
        case RE_ET_BTN_RELEASED:
            ESP_LOGI(TAG, "Button released");
            break;
        case RE_ET_BTN_CLICKED:
            ESP_LOGI(TAG, "Button clicked");
            editing = !editing;
            step_ind = 0;
            break;
        case RE_ET_BTN_LONG_PRESSED:
            ESP_LOGI(TAG, "Looooong pressed button");
            step_ind = step_ind == 0 ? 1 : 0;
            break;
        case RE_ET_CHANGED:
        {
            int8_t sign = (e.diff > 0 ? 1 : -1);
            if (editing)
            {
                // Range selected, edit it
                sel_range->value += sel_range->steps[step_ind] * sign;
                if (sel_range->value < sel_range->min_value)
                    sel_range->value = sel_range->min_value;
                else if (sel_range->value > sel_range->max_value)
                    sel_range->value = sel_range->max_value;

                if (pwm_get_mode() == PWM_MANUAL)
                    pwm_manual_update(prf_range.value, pd_range.value);
                else if (pwm_get_mode() == PWM_AUDIO)
                    // TODO: EDIT THIS
                    audio_set_volume(pd_range.value * 255 / 100);
                ESP_LOGI(TAG, "Selected: %s, Value: %d", sel_range->name,
                         sel_range->value);
            }
            else
            {
                // Navigate between ranges

                lv_label_set_text(sel_range->name_txt, sel_range->name);

                sel_range_ind += sign;
                sel_range_ind = (sel_range_ind + range_count) % range_count;
                sel_range = ranges[sel_range_ind];

                char buf[12];
                sprintf(buf, ">%s<", sel_range->name);
                lv_label_set_text(sel_range->name_txt, buf);

                ESP_LOGI(TAG, "%s, Value: %d", sel_range->name,
                         sel_range->value);
            }
            char val_str[12];
            sprintf(val_str, "%d", sel_range->value);
            lv_label_set_text(sel_range->val_txt, val_str);
            break;
        }
        default:
            break;
        }

        // LVGL
        lv_timer_handler();
    }
}

// -----------------------------------------------------------------------------
// Function Definitions
// -----------------------------------------------------------------------------
void menu_init(void)
{
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = LCD_PIXEL_CLOCK_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_HOST, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_HOST, I2C_MODE_MASTER, 0, 0, 0));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = I2C_HW_ADDR,
        .control_phase_bytes = 1,       // According to SSD1306 datasheet
        .lcd_cmd_bits = LCD_CMD_BITS,   // According to SSD1306 datasheet
        .lcd_param_bits = LCD_CMD_BITS, // According to SSD1306 datasheet
        .dc_bit_offset = 6,             // According to SSD1306 datasheet
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(I2C_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {.bits_per_pixel = 1,
                                               .reset_gpio_num = GPIO_NUM_NC};
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {.io_handle = io_handle,
                                              .panel_handle = panel_handle,
                                              .buffer_size =
                                                  LCD_H_RES * LCD_V_RES,
                                              .double_buffer = true,
                                              .hres = LCD_H_RES,
                                              .vres = LCD_V_RES,
                                              .monochrome = true,
                                              .rotation = {
                                                  .swap_xy = false,
                                                  .mirror_x = false,
                                                  .mirror_y = false,
                                              }};
    disp = lvgl_port_add_disp(&disp_cfg);

    /* Rotation of the screen */
    lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);

    re_event_queue = xQueueCreate(5, sizeof(rotary_encoder_event_t));

    // Setup rotary encoder library
    ESP_ERROR_CHECK(rotary_encoder_init(re_event_queue));
    memset(&re, 0, sizeof(rotary_encoder_t));
    re.pin_a = PIN_RE_A, re.pin_b = PIN_RE_B, re.pin_btn = PIN_RE_SWT,
    rotary_encoder_add(&re);

    editing = false;
    step_ind = 0;
    sel_range_ind = 0;
    sel_range = ranges[sel_range_ind];

    ui_init();
    pd_range.val_txt = objects.dc_val_txt;
    prf_range.val_txt = objects.prf_val_txt;
    pd_range.name_txt = objects.dc_txt;
    prf_range.name_txt = objects.prf_txt;

    xTaskCreate(menu_task, "menu_task", 4096, NULL, 0, NULL);
}
