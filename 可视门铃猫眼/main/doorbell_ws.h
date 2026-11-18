#pragma once
#include "stdbool.h"
#include <stdint.h>

void doorbell_ws_init(void);
void doorbell_ws_audio_start(void);
void doorbell_ws_image_start(void);
void doorbell_ws_start_all(void);
void doorbell_ws_audio_close(void);
void doorbell_ws_image_close(void);
void doorbell_ws_close_all(void);
bool doorbell_ws_is_audio_connected(void);
bool doorbell_ws_is_image_connected(void);
void doorbell_ws_send_audio(uint8_t *, uint32_t);
void doorbell_ws_send_image(uint8_t *, uint32_t);