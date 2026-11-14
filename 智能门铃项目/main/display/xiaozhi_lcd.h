/**
 * @file xiaozhi_lcd.h
 * ST7789 底层初始化接口和供 LVGL 使用的面板句柄。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
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

extern esp_lcd_panel_handle_t panel_handle;

/**
 * @brief 初始化 SPI2 总线和 ST7789 屏幕，配置方向并开启背光。
 * RGB565 每像素占 2 字节；SPI 时钟为 40 MHz，背光在初始化期间暂时关闭。
 * 本层执行坐标轴交换和 X 方向镜像，上层 LVGL 分辨率需与最终方向配合。
 */
void xiaozhi_lcd_init(void);