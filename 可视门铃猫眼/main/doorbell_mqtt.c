#include "doorbell_mqtt.h"
#include "sdkconfig.h"
#include "cJSON.h"
#include "esp_log.h"

#define TAG "doorbell_mqtt"

doorbell_mqtt_callback_t doorbell_mqtt_callback;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:

        msg_id = esp_mqtt_client_subscribe(client, "/doorbell/cmd", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/doorbell/status", "{\"version\": \"v1.0\",\"status\":\"OK\"}", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        cJSON *root = cJSON_ParseWithLength(event->data, event->data_len);
        if (root == NULL)
        {
            ESP_LOGW(TAG, "woc 不是json");
            return;
        }
        cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
        if (cmd == NULL)
        {
            ESP_LOGW(TAG, "woc 没有cmd");
            cJSON_Delete(root);
            return;
        }
        if (strcmp(cmd->valuestring, "switch") == 0)
        {
            // 做点什么事
            if (doorbell_mqtt_callback != NULL)
            {
                doorbell_mqtt_callback();
            }
        }
        cJSON_Delete(root);

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void doorbell_mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI};

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void doorbell_mqtt_register_cb(doorbell_mqtt_callback_t cb)
{
    doorbell_mqtt_callback = cb;
}