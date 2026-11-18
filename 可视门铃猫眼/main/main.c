#include <stdio.h>

#include "doorbell_button.h"
#include "esp_log.h"
#include "doorbell_led.h"
#include "doorbell_codec.h"
#include "doorbell_wifi.h"
#include "doorbell_lcd.h"
#include "doorbell_camera.h"
#include "doorbell_mqtt.h"
#include "doorbell_ws.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

uint8_t *rgb565_buffer = NULL;

// 创建一个事件标志组 维护我所有的业务逻辑标志
EventGroupHandle_t doorbell_event_group = NULL;
SemaphoreHandle_t new_fb_sema;
SemaphoreHandle_t ws_streaming_complete_sema;

#define LCD_STREAMING_BIT BIT0
#define WS_STREAMING_BIT BIT1

#define TAG "app_main"

void button_cb(void *button_handle, void *user_data)
{
    uint8_t event = (uint8_t)user_data;
    if (event == 1)
    {
        // 前面的按键被单击了
        ESP_LOGI("button", "front button single click");
        doorbell_led_toggle();
        doorbell_codec_play_menling();
    }

    if (event == 2)
    {
        // 后面的按键被单击了
        ESP_LOGI("button", "back button single click");
        EventBits_t bits = xEventGroupGetBits(doorbell_event_group);
        if (bits & LCD_STREAMING_BIT)
        {
            xEventGroupClearBits(doorbell_event_group, LCD_STREAMING_BIT);
        }
        else
        {
            xEventGroupSetBits(doorbell_event_group, LCD_STREAMING_BIT);
        }
    }

    if (event == 3)
    {
        // 后面的按键被长按了
        ESP_LOGI("button", "back button long press start");
        doorbell_wifi_reset();
    }
}

void mqtt_cb(void)
{
    EventBits_t bits = xEventGroupGetBits(doorbell_event_group);
    if (bits & WS_STREAMING_BIT)
    {
        ESP_LOGI(TAG, "关闭ws推流");
        doorbell_ws_close_all();
        xEventGroupClearBits(doorbell_event_group, WS_STREAMING_BIT);
    }
    else
    {
        ESP_LOGI(TAG, "开启ws推流");
        doorbell_ws_start_all();
        xEventGroupSetBits(doorbell_event_group, WS_STREAMING_BIT);
    }
}

// 音频推流任务 将esp32上的音频数据 通过websocket发给服务器
uint8_t audio_buffer[2048] = {0};

void audio_streaming_task(void *pvParameters)
{
    while (1)
    {
        xEventGroupWaitBits(doorbell_event_group, WS_STREAMING_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        // 将数据发送给音频通道
        ESP_LOGI(TAG, "准备推送音频");
        if (doorbell_ws_is_audio_connected())
        {
            ESP_LOGI(TAG, "推送一次音频");
            // 读出麦克风数据
            doorbell_codec_record(audio_buffer, 2048);
            // 发送出去
            doorbell_ws_send_audio(audio_buffer, 2048);
        }
        else
        {
            vTaskDelay(50);
        }
    }
}

camera_fb_t *fb = NULL;
void image_streaming_task(void *pvParameters)
{
    while (1)
    {
        EventBits_t bits = xEventGroupWaitBits(doorbell_event_group, WS_STREAMING_BIT | LCD_STREAMING_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        // 1. 先读出图像数据
        fb = doorbell_camera_capture();
        // give 信号量1
        if (bits & WS_STREAMING_BIT)
        {
            xSemaphoreGive(new_fb_sema);
        }

        // 2. 判断到底给屏幕推还是给ws推
        if (bits & LCD_STREAMING_BIT)
        {
            doorbell_camera_jpeg2rgb565(fb->buf, fb->len, rgb565_buffer);
            doorbell_lcd_show_image(rgb565_buffer);
        }

        // take信号量2
        if (bits & WS_STREAMING_BIT)
        {
            xSemaphoreTake(ws_streaming_complete_sema, 500);
        }

        doorbell_camera_return_fb(fb);
    }
}

void image_slave_ws_streaming_task(void *pvParameters)
{
    while (1)
    {
        // take 信号量1
        xSemaphoreTake(new_fb_sema, portMAX_DELAY);
        // 推流
        if (doorbell_ws_is_image_connected())
        {
            doorbell_ws_send_image(fb->buf, fb->len);
        }
        xSemaphoreGive(ws_streaming_complete_sema);
        // give 信号量2
    }
}

uint8_t data[1024];
void app_main(void)
{

    // 0. 创建所有任务间通信的对象
    doorbell_event_group = xEventGroupCreate();
    new_fb_sema = xSemaphoreCreateBinary();
    ws_streaming_complete_sema = xSemaphoreCreateBinary();

    // 1. 先做各类模块的初始化
    // 1.1 按键初始化
    doorbell_button_init();
    doorbell_button_front_register_cb(BUTTON_SINGLE_CLICK, button_cb, (void *)1);
    doorbell_button_back_register_cb(BUTTON_SINGLE_CLICK, button_cb, (void *)2);
    doorbell_button_back_register_cb(BUTTON_LONG_PRESS_START, button_cb, (void *)3);
    // 1.2 led初始化
    doorbell_led_init();
    // 1.3 codec初始化
    doorbell_codec_init();
    // 1.4 WIFI初始化(阻塞式) 连不上网 会直接卡死在这个函数上
    doorbell_wifi_init();
    // 1.5 LCD屏初始化
    doorbell_lcd_init();
    // 1.6 摄像头的初始化
    doorbell_camera_init();
    // 1.7 mqtt模块的初始化
    doorbell_mqtt_register_cb(mqtt_cb);
    doorbell_mqtt_init();
    // 1.8 webscoket模块的初始化
    doorbell_ws_init();

    // 2. 做各种内存的分配
    rgb565_buffer = (uint8_t *)heap_caps_malloc(320 * 240 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);

    // 3. 开始创建各种业务相关的任务
    // 3.1 创建音频推流任务
    xTaskCreatePinnedToCore(audio_streaming_task, "audio_streaming_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(image_streaming_task, "image_streaming_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(image_slave_ws_streaming_task, "image_slave_ws_streaming_task", 4096, NULL, 5, NULL, 0);
}