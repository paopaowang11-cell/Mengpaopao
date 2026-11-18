#pragma once

#include "mqtt_client.h"

typedef void (*doorbell_mqtt_callback_t)(void);

void doorbell_mqtt_init(void);

void doorbell_mqtt_register_cb(doorbell_mqtt_callback_t cb);