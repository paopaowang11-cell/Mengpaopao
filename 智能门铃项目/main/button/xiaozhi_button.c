/**
 * @file xiaozhi_button.c
 * ADC 电阻分压按键封装：SW2/SW3 共用 ADC 通道，由不同电压区间区分。
 * 本层只创建按键和注册事件，按键对应的业务行为在 main.c 中定义。
 */
#include "button/xiaozhi_button.h"

#define TAG "xiaozhi_button"

// 两个句柄表示逻辑按键，物理上由同一路 ADC 的不同电压窗口区分。
button_handle_t sw2_adc_btn_handle;
button_handle_t sw3_adc_btn_handle;
/**
 * @brief 创建共享 ADC1 通道 7 的两个电阻分压按键。
 * 前键 SW2 的有效电压范围为 0～500 mV，后键 SW3 为 1000～2400 mV。
 * button_index 区分同一 ADC 通道的两个逻辑按键，去抖和长按判断由 button 组件处理。
 */
void xiaozhi_button_init(void)
{
    // 1. 创建sw2 ADC按键 按下之后是GND
    button_config_t sw2_btn_cfg = {0}; // 清零前键通用配置，按键的去抖、长按等默认处理交给组件。
    button_adc_config_t sw2_btn_adc_cfg = { // 定义 SW2 对应的 ADC 电压判定配置。
        .unit_id = ADC_UNIT_1, // 选择 ADC1，两个按键共享这一 ADC 单元。
        .adc_channel = 7, // 使用 ADC 通道 7，读取电阻分压产生的电压。
        .button_index = 0, // 把同通道上的 SW2 标记为逻辑按键 0。
        .min = 0, // 前键有效电压下限为 0 mV，按下时接近地电位。
        .max = 500}; // 前键有效电压上限为 500 mV，为实际电压波动留出范围。
    sw2_adc_btn_handle = NULL; // 创建前先清空 SW2 句柄，便于通过是否为空判断创建结果。

    iot_button_new_adc_device(&sw2_btn_cfg, &sw2_btn_adc_cfg, &sw2_adc_btn_handle); // 根据通用配置和 ADC 配置创建 SW2，把输出句柄写入全局变量。

    if (sw2_adc_btn_handle == NULL) // 检查 SW2 句柄是否仍为空，表示没有得到有效按键实例。
    {
        ESP_LOGE(TAG, "sw2 Button create failed"); // 打印前侧按键创建失败日志，当前流程随后仍会继续。
    }

    // -------------------------------------------------------

    // 2. 创建sw3 ADC 按键 1.65V
    button_config_t sw3_btn_cfg = {0}; // 清零后键通用配置，使用组件默认按键时序设置。
    button_adc_config_t sw3_btn_adc_cfg = { // 定义 SW3 对应的另一组 ADC 电压窗口。
        .unit_id = ADC_UNIT_1, // SW3 同样使用 ADC1，不再占用另一 ADC 单元。
        .adc_channel = 7, // SW3 同样使用通道 7，由不同电压而不是不同引脚区分。
        .button_index = 1, // 把 SW3 标记为该通道的逻辑按键 1，与 SW2 的编号区分。
        .min = 1000, // 后键有效电压下限为 1000 mV。
        .max = 2400}; // 后键有效电压上限为 2400 mV，覆盖按下时约 1.65 V 的电平。
    sw3_adc_btn_handle = NULL; // 创建前清空 SW3 句柄，方便随后检查。
    iot_button_new_adc_device(&sw3_btn_cfg, &sw3_btn_adc_cfg, &sw3_adc_btn_handle); // 创建 SW3 逻辑按键，把句柄写入 sw3_adc_btn_handle。
    if (sw3_adc_btn_handle == NULL) // 检查后侧按键实例是否创建成功。
    {
        ESP_LOGE(TAG, "sw3 Button create failed"); // 打印后侧按键创建失败日志，方便检查 ADC 或组件配置。
    }
}

// 给前面的按键(sw2)注册回调函数
/**
 * @brief 为前侧 SW2 按键注册指定事件的回调。
 * @param btn_event 事件类型，例如 BUTTON_SINGLE_CLICK。
 * @param cb 按键组件任务中执行的回调。
 * @param usr_data 用户参数，由组件原样传给回调；注册前先调用 xiaozhi_button_init。
 */
void xiaozhi_button_front_register_cb(button_event_t btn_event, button_cb_t cb, void *usr_data)
{
    iot_button_register_cb(sw2_adc_btn_handle, btn_event, NULL, cb, usr_data); // 在 SW2 上注册 btn_event 回调；NULL 不指定额外事件参数，usr_data 原样传给 cb。
}

// 给后面的按键(sw3)注册回调函数
/**
 * @brief 为后侧 SW3 按键注册指定事件的回调。
 * @param btn_event 单击、长按开始等 button 组件事件类型。
 * @param cb 事件触发时执行的回调。
 * @param usr_data 透传的用户参数，主程序用它区分事件来源。
 */
void xiaozhi_button_back_register_cb(button_event_t btn_event, button_cb_t cb, void *usr_data)
{
    iot_button_register_cb(sw3_adc_btn_handle, btn_event, NULL, cb, usr_data); // 在 SW3 上注册 btn_event 回调；同一回调可通过透传的 usr_data 区分具体事件。
}
