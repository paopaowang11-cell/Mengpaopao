/**
 * @file xiaozhi_hard_codec.h
 * ES8311 硬件音频接口：初始化、PCM 录放音、音量与静音控制。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once

#include "driver/i2c_master.h"

#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

/**
 * @brief 初始化 ES8311 硬件与 esp_codec_dev 录放音抽象设备。
 * I2C 接口负责控制寄存器，I2S 接口负责 PCM 数据，GPIO7 控制外部功放。
 * 打开 16 kHz、16 bit、单声道录放音，设置初始输出音量 90 和输入增益 15 dB。
 * 由 xiaozhi_sr_init 调用，应在开始录音、解码播放或远程音量控制前完成。
 */
void xiaozhi_hard_codec_init(void);

/**
 * @brief 把 PCM 字节数据写入音频设备，经 I2S 送往 ES8311 播放。
 * @param data PCM 缓冲区，格式应与设备打开时的 16 kHz/16 bit/单声道一致。
 * @param data_size 本次写入的字节数，不是采样点数；底层写入可能等待音频传输。
 */
void xiaozhi_hard_codec_play(uint8_t *data, uint32_t data_size);

/**
 * @brief 从 ES8311 录音通道读取 PCM 数据到调用方缓冲区。
 * @param data 调用方提供的可写缓冲区，至少容纳 data_size 字节。
 * @param data_size 请求读取的字节数；16 bit 单声道每个采样点占 2 字节。
 */
void xiaozhi_hard_codec_record(uint8_t *data, uint32_t data_size);

/**
 * @brief 设置输出音量，并记住此值用于取消静音时恢复。
 * @param volume 协议约定为 0～100 的音量值；当前封装直接传给驱动，不额外限幅。
 */
void xiaozhi_hard_codec_set_volume(int volume);
/**
 * @brief 通过调节输出音量实现静音或恢复音量。
 * @param mute true 设置音量为 0，false 恢复 last_volume。
 * last_volume 由 set_volume 更新，初值为 0，不会自动记录初始化时设置的音量 90。
 */
void xiaozhi_hard_codec_set_mute(bool mute);