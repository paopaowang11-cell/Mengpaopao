/**
 * @file xiaozhi_sr.h
 * ESP-SR 音频前端接口：初始化、麦克风喂数与识别结果处理任务。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once

/**
 * @brief 初始化音频硬件、ESP-SR 模型和 AFE 实例，启动采集与结果处理任务。
 * 从 model 分区加载模型，输入格式 M 表示单路麦克风，没有回声参考通道。
 * 关闭 AEC/SE/NS；配置 VAD 的持续时间门限及 WakeNet 检测模式，并优先使用 PSRAM。
 */
void xiaozhi_sr_init(void);

/**
 * @brief 从 AFE 获取处理后音频、VAD 结果和唤醒词结果。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * 检测到唤醒词后通知主业务；wake_flag 置位后才分派 VAD 状态变化并转发人声音频。
 * 优先写入 VAD 缓存，再写当前音频，减少检测延迟导致的句首丢失。
 * AFE 结果中的音频指针由 AFE 管理，本任务通过环形缓冲区复制数据，不释放这些指针。
 */
void xiaozhi_sr_fetch_task(void *arg);

/**
 * @brief 按 AFE 要求的块大小连续采集麦克风 PCM，并喂给音频前端。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * 块大小接口返回每通道采样点数，乘通道数与 sizeof(int16_t) 后才是录音字节数。
 * 工作缓冲区在任务启动时分配一次并反复使用，避免每轮采集都申请堆内存。
 */
void xiaozhi_sr_feed_task(void *arg);