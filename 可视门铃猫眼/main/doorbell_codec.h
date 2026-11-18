#pragma once

#include "driver/i2c_master.h"

#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

void doorbell_codec_init(void);

void doorbell_codec_play(uint8_t *data, uint32_t data_size);

void doorbell_codec_play_menling(void);

void doorbell_codec_record(uint8_t *data, uint32_t data_size);