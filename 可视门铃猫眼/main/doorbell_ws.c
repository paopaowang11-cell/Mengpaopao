#include "doorbell_ws.h"
#include "esp_websocket_client.h"
#include "sdkconfig.h"
#include "doorbell_codec.h"
#include "esp_log.h"

esp_websocket_client_handle_t audio_client_handle;
esp_websocket_client_handle_t image_client_handle;

#define TAG "doorbell_ws"

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    esp_websocket_client_handle_t current_client = (esp_websocket_client_handle_t)handler_args;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_BEGIN:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN : %s", current_client == audio_client_handle ? "audio" : "image");
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED : %s", current_client == audio_client_handle ? "audio" : "image");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED : %s", current_client == audio_client_handle ? "audio" : "image");
        break;
    case WEBSOCKET_EVENT_DATA:
        // ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA : %s", current_client == audio_client_handle ? "audio" : "image");
        if (current_client == image_client_handle)
        {
            break;
        }
        if (data->op_code != 0x2)
        {
            break;
        }

        doorbell_codec_play(data->data_ptr, data->data_len);

        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WEBSOCKET_EVENT_ERROR : %s", current_client == audio_client_handle ? "audio" : "image");
        break;
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGW(TAG, "WEBSOCKET_EVENT_FINISH : %s", current_client == audio_client_handle ? "audio" : "image");
        break;
    }
}

// 我要创建两个客户端
void doorbell_ws_init(void)
{
    esp_websocket_client_config_t aduio_websocket_cfg = {
        .uri = CONFIG_WEBSOCKET_SERVER_BASE_URL "/ws/esp_audio",
        .disable_auto_reconnect = false,
    };

    esp_websocket_client_config_t image_websocket_cfg = {
        .uri = CONFIG_WEBSOCKET_SERVER_BASE_URL "/ws/esp_image",
        .disable_auto_reconnect = false,
    };

    audio_client_handle = esp_websocket_client_init(&aduio_websocket_cfg);
    image_client_handle = esp_websocket_client_init(&image_websocket_cfg);

    esp_websocket_register_events(audio_client_handle, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)audio_client_handle);
    esp_websocket_register_events(image_client_handle, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)image_client_handle);
}

void doorbell_ws_audio_start(void)
{
    esp_websocket_client_start(audio_client_handle);
}

void doorbell_ws_image_start(void)
{
    esp_websocket_client_start(image_client_handle);
}

void doorbell_ws_start_all(void)
{
    doorbell_ws_audio_start();
    doorbell_ws_image_start();
}

void doorbell_ws_audio_close(void)
{
    esp_websocket_client_close(audio_client_handle, 1000);
}

void doorbell_ws_image_close(void)
{
    esp_websocket_client_close(image_client_handle, 1000);
}

void doorbell_ws_close_all(void)
{
    doorbell_ws_audio_close();
    doorbell_ws_image_close();
}

bool doorbell_ws_is_audio_connected(void)
{
    return esp_websocket_client_is_connected(audio_client_handle);
}

bool doorbell_ws_is_image_connected(void)
{
    return esp_websocket_client_is_connected(image_client_handle);
}
void doorbell_ws_send_audio(uint8_t *data_buffer, uint32_t data_size)
{
    esp_websocket_client_send_bin(audio_client_handle, (const char *)data_buffer, data_size, 1000);
}

void doorbell_ws_send_image(uint8_t *data_buffer, uint32_t data_size)
{
    esp_websocket_client_send_bin(image_client_handle, (const char *)data_buffer, data_size, 1000);
}
