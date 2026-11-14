/**
 * @file xiaozhi_sr.c
 * ESP-SR 音频前端（AFE）接入层。
 * feed 任务采集音频，fetch 任务取得唤醒词/VAD/处理后 PCM 并分派业务回调。
 * 本地负责唤醒与人声检测，完整语音识别和对话由服务器完成。
 */
#include "xiaozhi_sr.h"

#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include "audio/xiaozhi_hard_codec.h"
#include "esp_log.h"
#include "common/common_types.h"

#define TAG "xiaozhi_sr"

// afe_handle 是算法接口函数表，afe_data 是依照配置创建的具体处理实例。
static esp_afe_sr_iface_t *afe_handle = NULL;
esp_afe_sr_data_t *afe_data = NULL;

/**
 * @brief 初始化音频硬件、ESP-SR 模型和 AFE 实例，启动采集与结果处理任务。
 * 从 model 分区加载模型，输入格式 M 表示单路麦克风，没有回声参考通道。
 * 关闭 AEC/SE/NS；配置 VAD 的持续时间门限及 WakeNet 检测模式，并优先使用 PSRAM。
 */
void xiaozhi_sr_init(void)
{
    // 0. 初始化ES8311

    xiaozhi_hard_codec_init(); // 先打开 ES8311 及其 I2C/I2S 外设，确保后续任务能够采集麦克风音频。

    // 1. 初始化模型
    srmodel_list_t *models = esp_srmodel_init("model"); // 从名为 model 的 Flash 数据分区加载语音模型列表。
    // 2. 初始化配置
    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF); // 按单麦克风 M 输入创建语音识别 AFE 配置，选择高性能模式。

    // 关闭一些硬件上本来就不支持的功能 免得出现反效果
    afe_config->aec_init = false; // 关闭声学回声消除 AEC，当前输入没有提供扬声器参考通道。
    afe_config->se_init = false; // 关闭 SE 语音增强阶段，沿用本项目的音频前端配置。
    afe_config->ns_init = false; // 关闭 NS 噪声抑制阶段，不在本配置下启用该处理。

    // 静音至少持续 500 ms、人声至少持续 128 ms 才满足对应的 VAD 持续时间配置。
    afe_config->vad_min_noise_ms = 500; // 设置噪声或静音的最短持续时间为 500 ms。
    afe_config->vad_min_speech_ms = 128; // 设置人声的最短持续时间为 128 ms。
    afe_config->vad_mode = VAD_MODE_2; // 选择 VAD_MODE_2 人声判定模式，具体门限由库定义。

    // 使用 WakeNet 的 DET_MODE_95 检测模式；实际阈值行为由所选模型和库实现决定。
    afe_config->wakenet_mode = DET_MODE_95; // 选择 WakeNet 的 DET_MODE_95 检测模式。
    // 优先把 AFE 的可分配工作内存放到外部 PSRAM，减轻内部 RAM 压力。
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM; // 让 AFE 尽可能把工作内存分配到外部 PSRAM，减少内部 RAM 压力。

    // 3. 获取afe（音频前端的句柄）
    afe_handle = esp_afe_handle_from_config(afe_config); // 根据配置取得对应 AFE 算法接口函数表。
    // 4. 创建实例
    afe_data = afe_handle->create_from_config(afe_config); // 调用接口创建具体 AFE 实例，供 feed/fetch 两个任务共享使用。

    // 启动喂数据和取数据的任务
    // 采集任务在核心 0、优先级 15；取结果任务在核心 1、优先级 5，栈大小单位为字节。
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_sr_feed_task, "feed_task", 8 * 1024, NULL, 15, NULL, 0, MALLOC_CAP_SPIRAM); // 创建采集喂数任务：8 KiB PSRAM 栈、优先级 15、核心 0。
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_sr_fetch_task, "fetch_task", 4 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM); // 创建结果取数任务：4 KiB PSRAM 栈、优先级 5、核心 1。
}

/**
 * @brief 按 AFE 要求的块大小连续采集麦克风 PCM，并喂给音频前端。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * 块大小接口返回每通道采样点数，乘通道数与 sizeof(int16_t) 后才是录音字节数。
 * 工作缓冲区在任务启动时分配一次并反复使用，避免每轮采集都申请堆内存。
 */
void xiaozhi_sr_feed_task(void *arg)
{
    // 喂数据 不停地喂
    // 1. 根据模型地配置信息 得知这个模型一次需要多少采样点的数据来喂
    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data); // 询问当前 AFE 每次喂数需要的每通道采样点数。
    // 2. 这个模型有几张嘴（需要几个通道的数据）
    int feed_channel_num = afe_handle->get_feed_channel_num(afe_data); // 询问 AFE 输入通道数，避免把采样点数直接误当总字节数。
    // 3. 从堆里分配一个空间 当作喂数据的缓冲区
    int16_t *feed_buffer = malloc(feed_chunksize * feed_channel_num * sizeof(int16_t)); // 按采样点数×通道数×2 字节分配录音工作区，以 int16_t 保存 PCM。
    while (1) // 连续采集并喂数，录音等待会让任务随音频节奏运行。
    {
        // 4. 从es8311里获取麦克风数据
        xiaozhi_hard_codec_record((uint8_t *)feed_buffer, feed_chunksize * feed_channel_num * sizeof(int16_t)); // 按完整块字节数读取 ES8311 PCM；转换指针类型仅适配字节接口，不改变数据。
        // 5. 把数据喂到模型里
        afe_handle->feed(afe_data, feed_buffer); // 把录到的这块 PCM 交给 AFE，唤醒词、VAD 等结果由 fetch 任务取得。
    }
}

/**
 * @brief 从 AFE 获取处理后音频、VAD 结果和唤醒词结果。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * 检测到唤醒词后通知主业务；wake_flag 置位后才分派 VAD 状态变化并转发人声音频。
 * 优先写入 VAD 缓存，再写当前音频，减少检测延迟导致的句首丢失。
 * AFE 结果中的音频指针由 AFE 管理，本任务通过环形缓冲区复制数据，不释放这些指针。
 */
void xiaozhi_sr_fetch_task(void *arg)
{
    while (1) // 持续获取 AFE 的处理结果，驱动唤醒、VAD 和音频转发。
    {
        // 1. 从模型里获取结果
        afe_fetch_result_t *result = afe_handle->fetch(afe_data); // 从 AFE 取得一份结果，其中包含音频地址、有效长度及检测状态。
        // 2. result->data这个成员是 处理后的数据
        int16_t *processed_audio = result->data; // 借用结果内的处理后 PCM 地址，不接管其内存所有权。
        // 3. 有没有人声
        vad_state_t vad_state = result->vad_state; // 取出本块音频的人声/静音判定结果。
        // 4. 有没有唤醒词
        wakenet_state_t wakeup_state = result->wakeup_state; // 取出本块音频的 WakeNet 唤醒词检测状态。

        if (wakeup_state == WAKENET_DETECTED) // 只有检测结果明确为 WAKENET_DETECTED 时才执行唤醒分支。
        {
            ESP_LOGI(TAG, "检测到了唤醒词!"); // 打印已检测到唤醒词的日志，便于串口观察触发情况。
            // 下面这两行 可加可不加 加了更好 主要是为了在唤醒后精准地触发一次 人声状态改变的回调。
            xiaozhi_handle.last_vad_state = VAD_SILENCE; // 将上一轮 VAD 状态设为静音，重建唤醒后的状态变化比较基准。
            xiaozhi_handle.current_vad_state = VAD_SILENCE; // 同步把当前 VAD 状态设为静音，后续再用新的人声结果更新。
            // 执行回调
            if (xiaozhi_handle.wake_callback != NULL) // 检查主程序是否注册了唤醒回调，避免调用空函数指针。
            {
                xiaozhi_handle.wake_callback(); // 调用主业务唤醒回调，本项目由它通知独立唤醒任务。
            }
        }

        // 我们关注的是关键词唤醒后的 人声状态的改变
        if (xiaozhi_handle.wake_flag == true) // 只有业务握手完成、wake_flag 为真时才处理 VAD 状态变化。
        {
            // 更新当前的状态 而且这个地方 唤醒后才会更新
            xiaozhi_handle.current_vad_state = vad_state; // 把本轮 VAD 结果写入全局句柄，供业务回调读取。
            if (xiaozhi_handle.current_vad_state != xiaozhi_handle.last_vad_state) // 对比当前与上次状态，只在状态改变时通知业务，避免每块音频重复通知。
            {

                if (xiaozhi_handle.vad_changed_callback != NULL) // 确认 VAD 状态变化回调已经注册。
                {
                    xiaozhi_handle.vad_changed_callback(); // 执行回调，按人声开始/静音结束切换监听状态并发送协议消息。
                }
            }
            // 把当前状态存为上一次状态
            xiaozhi_handle.last_vad_state = xiaozhi_handle.current_vad_state; // 处理后保存当前状态，为下一块音频的状态比较做准备。
        }

        // 什么时候才做数据的发送?
        // 1. 既要唤醒过 也要有人声 满足这两个条件的音频才有必要向后传递
        if (xiaozhi_handle.wake_flag == true && vad_state == VAD_SPEECH) // 仅在已经唤醒且本块有人声时，才向 Opus 编码链路转发音频。
        {
            // 若存在 VAD 预缓存，先补上检测到人声前缓存的音频，保留句首内容。
            if (result->vad_cache_size > 0) // 判断 AFE 是否保留了人声触发前的缓存，避免丢掉语句开头。
            {
                int16_t *vad_cache = result->vad_cache; // 取出 VAD 缓存音频地址，该内存仍由 AFE 管理。
                // vad_cache_size 与 data_size 的单位均为字节，直接传给缓冲区发送接口。
                xRingbufferSend(xiaozhi_handle.sr2encoder_ringbuffer_handle, vad_cache, result->vad_cache_size, portMAX_DELAY); // 先复制 vad_cache_size 字节的预缓存到 PCM 环形缓冲；满时等待。
            }
            xRingbufferSend(xiaozhi_handle.sr2encoder_ringbuffer_handle, processed_audio, result->data_size, portMAX_DELAY); // 再复制 data_size 字节的当前 PCM，使预缓存与当前块按时间顺序排列。
        }
    }
}
