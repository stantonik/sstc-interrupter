/*
 * Copyright (C) 2025 Stanley Arnaud <stantonik@stantonik-mba.local>
 *
 * Distributed under terms of the MIT license.
 */

/**
 * @file midi.c
 * @brief
 *
 * @author Stanley Arnaud
 * @date 10/01/2025
 * @version 0
 */

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------
#include "midi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "usb/usb_host.h"
#include <string.h>

// -----------------------------------------------------------------------------
// Macros and Constants
// -----------------------------------------------------------------------------
#define ACTION_OPEN_DEV (1 << 1)
#define ACTION_CLOSE_DEV (1 << 2)

// MIDI message are 4byte and can be packed up to a 64bytes USB packet
#define MIDI_PACKET_SIZE 64
#define MIDI_EP_ADDR 0x81
#define MIDI_INTERFACE_NB 1

#define HOST_TASK_PRIO 2
#define CLIENT_TASK_PRIO 1
#define HOST_TASK_STACK (5 * 1024)
#define CLIENT_TASK_STACK (5 * 1024)

#define TAG "midi"

// -----------------------------------------------------------------------------
// Static Variables
// -----------------------------------------------------------------------------
volatile bool transfer_active = false;

static midi_event_data_t event_data = {0};
static void (*midi_event_callback)(midi_event_data_t *event) = NULL;

static TaskHandle_t usb_host_task_hdl, usb_client_task_hdl;

struct class_driver_control
{
    uint32_t actions;
    uint8_t dev_addr;
    usb_host_client_handle_t client_hdl;
    usb_device_handle_t dev_hdl;
};

// -----------------------------------------------------------------------------
// Static Function Declarations
// -----------------------------------------------------------------------------
static void usb_host_task(void *pvParams);
static void usb_client_task(void *pvParams);

static void midi_in_cb(usb_transfer_t *transfer);
static void client_event_cb(const usb_host_client_event_msg_t *event_msg,
                            void *arg);

// -----------------------------------------------------------------------------
// Function Definitions
// -----------------------------------------------------------------------------
void midi_init(void)
{
    BaseType_t task_created;
    // Create USB host task
    task_created =
        xTaskCreatePinnedToCore(usb_host_task, "usb_host", HOST_TASK_STACK,
                                xTaskGetCurrentTaskHandle(), HOST_TASK_PRIO, &usb_host_task_hdl, 0);
    assert(task_created == pdPASS);

    ulTaskNotifyTake(false, 1000);

    // Create USB client (class driver) task
    task_created = xTaskCreatePinnedToCore(
        usb_client_task, "usb_client", CLIENT_TASK_STACK, NULL,
        CLIENT_TASK_PRIO, &usb_client_task_hdl, 0);
    assert(task_created == pdPASS);
}

static void usb_client_task(void *pvParams)
{
    // Initialize class driver objects
    static struct class_driver_control class_driver_obj = {0};
    // Register the client
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = &class_driver_obj,
        }};
    esp_err_t ret =
        usb_host_client_register(&client_config, &class_driver_obj.client_hdl);
    ESP_ERROR_CHECK(ret);

    // Allocate a USB transfer
    usb_transfer_t *transfer;
    usb_host_transfer_alloc(MIDI_PACKET_SIZE, 0, &transfer);
    memset(transfer->data_buffer, 0xAA, MIDI_PACKET_SIZE);
    transfer->num_bytes = MIDI_PACKET_SIZE;
    transfer->bEndpointAddress = MIDI_EP_ADDR;
    transfer->callback = midi_in_cb;
    transfer->context = (void *)&class_driver_obj;

    while (1)
    {
        usb_host_client_handle_events(class_driver_obj.client_hdl,
                                      portMAX_DELAY);
        if (class_driver_obj.actions & ACTION_OPEN_DEV)
        {
            ESP_LOGI(TAG, "USB device connected");
            // Open the device and claim interface 1
            esp_err_t err = usb_host_device_open(class_driver_obj.client_hdl,
                                                 class_driver_obj.dev_addr,
                                                 &class_driver_obj.dev_hdl);
            err = usb_host_interface_claim(class_driver_obj.client_hdl,
                                           class_driver_obj.dev_hdl,
                                           MIDI_INTERFACE_NB, 0);
            if (err == ESP_OK)
            {
                usb_device_info_t dev_info;
                ESP_ERROR_CHECK(
                    usb_host_device_info(class_driver_obj.dev_hdl, &dev_info));
                const usb_str_desc_t *str_desc = dev_info.str_desc_product;
                char buf[64] = {0};
                int n = 0;
                for (int i = 0; i < str_desc->bLength / 2; i++)
                {
                    /*
                    USB String descriptors of UTF-16.
                    Right now We just skip any character larger than 0xFF to
                    stay in BMP Basic Latin and Latin-1 Supplement range.
                    */
                    if (str_desc->wData[i] > 0xFF)
                    {
                        continue;
                    }
                    buf[n] = (char)str_desc->wData[i];
                    n++;
                }
                if (n > 64) n = 64;
                event_data.dev_name = malloc(n + 1);
                memcpy(event_data.dev_name, buf, n);
                event_data.dev_name[n] = '\0';
                ESP_LOGI(TAG, "found MIDI interface on device: %s", buf);

                event_data.type = MIDI_EVENT_DEV_CONNECTED;
                if (midi_event_callback) midi_event_callback(&event_data);

                transfer->device_handle = class_driver_obj.dev_hdl;
                transfer_active = true;
                usb_host_transfer_submit(transfer);
            }
            else
            {
                ESP_LOGI(TAG, "No MIDI endpoint found. Removing device.");
                class_driver_obj.actions |= ACTION_CLOSE_DEV;
            }
        }
        if (class_driver_obj.actions & ACTION_CLOSE_DEV)
        {
            transfer_active = false;
            transfer->device_handle = NULL;
            vTaskDelay(pdMS_TO_TICKS(100));
            usb_host_interface_release(class_driver_obj.client_hdl,
                                       class_driver_obj.dev_hdl,
                                       MIDI_INTERFACE_NB);
            usb_host_device_close(class_driver_obj.client_hdl,
                                  class_driver_obj.dev_hdl);

            ESP_LOGI(TAG, "USB device disconnected");
            free(event_data.dev_name);
            event_data.dev_name = NULL;
            event_data.type = MIDI_EVENT_DEV_DISCONNECTED;
            if (midi_event_callback) midi_event_callback(&event_data);
        }

        class_driver_obj.actions = 0;
    }

    // Cleanup class driver
    usb_host_transfer_free(transfer);
    usb_host_client_deregister(class_driver_obj.client_hdl);
}

static void usb_host_task(void *pvParams)
{
    ESP_LOGI(TAG, "Installing USB Host Library");
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    xTaskNotifyGive(pvParams);

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients)
    {
        uint32_t event_flags;
        ESP_ERROR_CHECK(
            usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            if (ESP_OK == usb_host_device_free_all())
                has_clients = false;
            else
                has_devices = true;
        }
        if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            has_clients = false;
        }
    }

    // Uninstall the USB Host Library
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskSuspend(NULL);
}

inline bool midi_is_connected() { return transfer_active; }

const char *midi_get_device_name(void)
{
    if (!transfer_active)
    {
        return NULL;
    }

    return event_data.dev_name;
}

void midi_set_event_callback(void (*cb)(midi_event_data_t *event))
{
    midi_event_callback = cb;
}

void midi_free(void)
{
    // Uninstall the USB Host Library
    ESP_ERROR_CHECK(usb_host_uninstall());
    // Deregister client
}

// -----------------------------------------------------------------------------
// Static Function Definitions
// -----------------------------------------------------------------------------
static void client_event_cb(const usb_host_client_event_msg_t *event_msg,
                            void *arg)
{
    struct class_driver_control *class_driver_obj =
        (struct class_driver_control *)arg;
    switch (event_msg->event)
    {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        class_driver_obj->dev_addr = event_msg->new_dev.address;
        class_driver_obj->dev_hdl = NULL;
        class_driver_obj->actions |= ACTION_OPEN_DEV;
        break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        class_driver_obj->actions |= ACTION_CLOSE_DEV;
        break;
    default:
        ESP_LOGI(TAG, "oula");
        abort();
    }
}

static void midi_in_cb(usb_transfer_t *transfer)
{
    for (int i = 0; i < transfer->actual_num_bytes; i += 4)
    {
        uint8_t cin = transfer->data_buffer[i] & 0x0F;
        uint8_t note = transfer->data_buffer[i + 2];
        uint8_t vel = transfer->data_buffer[i + 3];

        midi_message_t msg = {.note = note};

        switch (cin)
        {
        case 0x9: // Note On
            ESP_LOGI("MIDI", "Note On: %d Velocity %d", note, vel);
            msg.state = 1;
            msg.velocity = vel;
            break;
        case 0x8: // Note Off
            ESP_LOGI("MIDI", "Note Off: %d", note);
            break;
        default:
            break;
        }

        event_data.type = MIDI_EVENT_MSG_RECEIVED;
        event_data.msg = msg;
        if (midi_event_callback) midi_event_callback(&event_data);
    }

    if (transfer->actual_num_bytes > 0 && transfer_active &&
        transfer->device_handle != NULL)
    {
        esp_err_t ret = usb_host_transfer_submit(transfer);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to resubmit: %d", ret);
            transfer_active = false;
        }
    }
}
