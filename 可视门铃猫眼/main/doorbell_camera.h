#pragma once

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_camera.h"

void doorbell_camera_init(void);

camera_fb_t *doorbell_camera_capture(void);

void doorbell_camera_jpeg2rgb565(uint8_t *jpeg_data, uint32_t jpeg_len, uint8_t *rgb565_buffer);

void doorbell_camera_return_fb(camera_fb_t *pic);