#pragma once

#include "iot_button.h"
#include "button_gpio.h"
#include "button_adc.h"
#include "esp_log.h"

void doorbell_button_init(void);

void doorbell_button_front_register_cb(button_event_t btn_event, button_cb_t cb, void *user_data);

void doorbell_button_back_register_cb(button_event_t btn_event, button_cb_t cb, void *user_data);