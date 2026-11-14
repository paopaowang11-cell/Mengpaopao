/**
 * @file xiaozhi_button.h
 * ADC 按键接口：创建前后按键，并注册单击、长按等事件回调。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once

#include "button_gpio.h"
#include "iot_button.h"
#include "button_adc.h"
#include "esp_log.h"

/**
 * @brief 为后侧 SW3 按键注册指定事件的回调。
 * @param btn_event 单击、长按开始等 button 组件事件类型。
 * @param cb 事件触发时执行的回调。
 * @param usr_data 透传的用户参数，主程序用它区分事件来源。
 */
void xiaozhi_button_back_register_cb(button_event_t btn_event, button_cb_t cb, void *usr_data);

/**
 * @brief 为前侧 SW2 按键注册指定事件的回调。
 * @param btn_event 事件类型，例如 BUTTON_SINGLE_CLICK。
 * @param cb 按键组件任务中执行的回调。
 * @param usr_data 用户参数，由组件原样传给回调；注册前先调用 xiaozhi_button_init。
 */
void xiaozhi_button_front_register_cb(button_event_t btn_event, button_cb_t cb, void *usr_data);

/**
 * @brief 创建共享 ADC1 通道 7 的两个电阻分压按键。
 * 前键 SW2 的有效电压范围为 0～500 mV，后键 SW3 为 1000～2400 mV。
 * button_index 区分同一 ADC 通道的两个逻辑按键，去抖和长按判断由 button 组件处理。
 */
void xiaozhi_button_init(void);