#pragma once

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>

#include "jpeg_decoder.h"

void doorbell_lcd_init(void);

void doorbell_lcd_show_image(uint8_t *image_data);

void doorbell_lcd_test(void);