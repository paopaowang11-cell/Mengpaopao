/**
 * @file xiaozhi_opus_decoder.h
 * 下行 Opus 解码模块入口，初始化时创建后台解码播放任务。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once

/**
 * @brief 创建 16 kHz 单声道、60 ms 帧配置的 Opus 解码器并启动解码任务。
 * self_delimited=false 表示不使用 Opus 自分界格式，输入包边界由上层传输与缓冲区提供。
 */
void xiaozhi_opus_decoder_init(void);