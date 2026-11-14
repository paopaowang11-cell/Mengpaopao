/**
 * @file xiaozhi_wifi.h
 * BLE 配网与 Wi-Fi 接口：等待网络就绪，以及恢复配置并重启。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <wifi_provisioning/manager.h>

#include <wifi_provisioning/scheme_ble.h>

#include "qrcode.h"

/**
 * @brief 初始化网络栈、事件循环与 BLE 配网服务，并等待 STA 获取 IP。
 * 未配网时显示二维码等待手机传入凭据；已配网时直接使用保存的配置连接。
 * 这是阻塞式启动流程，xEventGroupWaitBits 使用 portMAX_DELAY，没有初始化超时。
 * 返回只说明启动阶段曾获得 IP，后续断线由事件回调自动尝试重连。
 */
void xiaozhi_wifi_init(void);

/**
 * @brief 恢复 Wi-Fi 驱动默认配置并立即重启设备，供下次启动重新配网。
 * 此处调用 esp_wifi_restore，不执行整片 Flash 擦除；函数正常情况下不会返回。
 */
void xiaozhi_wifi_reset(void);
