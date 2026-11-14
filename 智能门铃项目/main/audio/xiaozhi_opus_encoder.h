/**
 * @file xiaozhi_opus_encoder.h
 * 上行 Opus 编码模块入口，初始化时创建后台编码任务。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once

/**
 * @brief 创建 16 kHz 单声道 Opus 编码器、输入输出工作区及编码任务。
 * 参数：16 bit PCM、60 ms/帧、32 kbit/s、复杂度 5，关闭 FEC、DTX 和 VBR。
 * PCM 输入每帧为 16000 × 0.060 × 1 × 2 = 1920 字节，缓冲区尺寸以库查询结果为准。
 */
void xiaozhi_opus_encoder_init(void);