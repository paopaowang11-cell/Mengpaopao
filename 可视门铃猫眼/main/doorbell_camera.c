#include "doorbell_camera.h"

#define TAG "doorbell_camera"
#define CAM_PIN_PWDN -1  // power down is not used
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK 37

#define CAM_PIN_D7 38 // 9
#define CAM_PIN_D6 36 // 8
#define CAM_PIN_D5 35 // 7
#define CAM_PIN_D4 33 // 6
#define CAM_PIN_D3 10 // 5
#define CAM_PIN_D2 12 // 4
#define CAM_PIN_D1 11 // 3
#define CAM_PIN_D0 9  // 2
#define CAM_PIN_VSYNC 41
#define CAM_PIN_HREF 39
#define CAM_PIN_PCLK 34

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = -1,
    .pin_sccb_scl = -1,
    .sccb_i2c_port = 0, // 用的是I2C0

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,   // QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 12, // 0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 2,      // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

void doorbell_camera_init(void)
{

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
    }
}

camera_fb_t *doorbell_camera_capture(void)
{
    camera_fb_t *pic = esp_camera_fb_get();
    return pic;
}

void doorbell_camera_return_fb(camera_fb_t *pic)
{
    esp_camera_fb_return(pic);
}

void doorbell_camera_jpeg2rgb565(uint8_t *jpeg_data, uint32_t jpeg_len, uint8_t *rgb565_buffer)
{
    // JPEG decode config
    esp_jpeg_image_cfg_t jpeg_cfg = {
        .indata = jpeg_data,
        .indata_size = jpeg_len,
        .outbuf = rgb565_buffer,
        .outbuf_size = 320 * 240 * sizeof(uint16_t),
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            .swap_color_bytes = 1,
        }};

    // JPEG decode
    esp_jpeg_image_output_t outimg;
    esp_jpeg_decode(&jpeg_cfg, &outimg);
}