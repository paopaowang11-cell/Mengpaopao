/**
 * @file xiaozhi_opus_decoder.c
 * 下行音频解码模块，把服务器 Opus 包还原为 PCM 并交给硬件播放。
 * 输入环形缓冲区保存完整数据项，解码过程借用数据项内存，处理后必须归还。
 */
#include "audio/xiaozhi_opus_decoder.h"
#include "esp_opus_dec.h"
#include "common/common_types.h"
#include "esp_heap_caps.h"
#include "audio/xiaozhi_hard_codec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_audio_dec_handle_t decoder_handle;

void xiaozhi_opus_decoder_task(void *arg);
/**
 * @brief 创建 16 kHz 单声道、60 ms 帧配置的 Opus 解码器并启动解码任务。
 * self_delimited=false 表示不使用 Opus 自分界格式，输入包边界由上层传输与缓冲区提供。
 */
void xiaozhi_opus_decoder_init(void)
{
    esp_opus_dec_cfg_t cfg = { // 定义本次创建 Opus 解码器使用的配置结构。
        .sample_rate = ESP_AUDIO_SAMPLE_RATE_16K, // 解码输出采样率设为 16 kHz，与硬件播放设备一致。
        .channel = ESP_AUDIO_MONO, // 解码输出采用单声道。
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS, // 设置每帧音频时长为 60 ms，与协议声明保持一致。
        .self_delimited = false, // 不启用 Opus 自分界格式，输入包边界由上层提供。
    };

    esp_opus_dec_open(&cfg, sizeof(cfg), &decoder_handle); // 创建解码器，传入配置地址及字节大小，并接收 decoder_handle。

    // 解码任务固定到核心 0，优先级 5，PSRAM 栈大小为 16 KiB。
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_opus_decoder_task, "xiaozhi_opus_decoder_task", 16 * 1024, NULL, 5, NULL, 0, MALLOC_CAP_SPIRAM); // 创建常驻解码任务：16 KiB PSRAM 栈、优先级 5、核心 0，不保存任务句柄。
}

/**
 * @brief 消费下行 Opus 数据项，解码为 PCM 并交给 ES8311 播放。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * 每次解码后根据 consumed 移动输入指针，只把 decoded_size 字节交给播放接口。
 * 处理完整个数据项后归还环形缓冲区；8 KiB 输出工作区在常驻任务中重复使用。
 * 当前循环按解码成功且 consumed 向前推进的正常输入路径工作。
 */
void xiaozhi_opus_decoder_task(void *arg)
{
    // 1. 这是一个输出PCM数据结构体
    // 我们给他分配8k内存 其实如果是60ms音频 16k采样率 一个帧只会输出1920字节。
    // 此处预留 8 KiB 容量；当前解码器仍配置为 16 kHz，缓冲区变大不会自动改变输出采样率。
    esp_audio_dec_out_frame_t output_pcm_frame = {0}; // 清零 PCM 输出描述结构，稍后为其配置可写工作缓冲区。
    output_pcm_frame.buffer = heap_caps_malloc(8 * 1024, MALLOC_CAP_SPIRAM); // 从 PSRAM 申请 8 KiB 输出空间，整个任务运行期间反复使用。
    output_pcm_frame.len = 8 * 1024; // 告诉解码器输出缓冲区容量为 8192 字节，避免把容量误当实际输出长度。

    // 2. 这个是解码器的输入帧(opus格式) 里面的buffer和len我们不赋值 待会儿从ringbuffer里拿
    esp_audio_dec_in_raw_t input_opus_frame = {0}; // 清零 Opus 输入描述结构，数据地址与长度稍后来自环形缓冲区。

    // 3. 解码信息输出结构，可接收采样率、通道数等音频信息；当前业务未继续使用这些字段。
    esp_audio_dec_info_t dec_info = {0}; // 清零解码信息输出结构，供解码器填写采样率、声道等结果。

    while (1) // 任务持续循环，逐个接收并播放服务器的音频包。
    {
        // 4. 拿一个临时变量存一下要解码的数据长度
        size_t to_decode_data_len = 0; // 初始化本次收到的数据项长度，接收函数会填写实际字节数。
        // 5.从环形缓冲区里拿到要解析的数据的长度和起始位置的指针。
        void *to_decode_data_pointer = xRingbufferReceive(xiaozhi_handle.decoder_in_ring_h, &to_decode_data_len, portMAX_DELAY); // 等待下行 Opus 数据项；没有数据时无限阻塞，返回指针由缓冲区管理。
        // 6. 把要解码的数据长度和数据指针挂到 input_frame里去
        input_opus_frame.buffer = to_decode_data_pointer; // 把借用的数据项地址交给解码输入结构，不额外复制音频。
        input_opus_frame.len = to_decode_data_len; // 填写该数据项的有效字节数，作为本轮尚待解码的输入长度。

        // 7. 判断是否还有数据要进行解码
        while (input_opus_frame.len > 0) // 只要输入还有未消耗字节，就继续执行解码处理。
        {
            // 8. 这个函数调一次只会解码一个帧，它会在input_opus_frame的buffer里取出一个帧
            // 并且更新input_opus_frame中的consumed变量，相当于告诉我们它解码的这个帧有多长
            esp_opus_dec_decode(decoder_handle, &input_opus_frame, &output_pcm_frame, &dec_info); // 解码当前输入，输出 PCM、有效输出大小及本次 consumed 字节数。

            // 9.解码后的数据是PCM格式的 可以直接扔给es8311播放
            xiaozhi_hard_codec_play(output_pcm_frame.buffer, output_pcm_frame.decoded_size); // 只播放 decoded_size 个有效 PCM 字节，而不是整个输出缓冲区容量。
            // 10. 把缓冲区指针向后移，长度往小减。这样就可以取出下一个帧了
            input_opus_frame.buffer += input_opus_frame.consumed; // 把输入指针移过本次已经消耗的字节，下轮从后续位置开始。
            input_opus_frame.len -= input_opus_frame.consumed; // 从剩余长度中扣除 consumed，使指针和可用长度同步推进。
        }
        // 11. 记得释放ringbuffer 否则websocket没法继续往里面塞数据。
        vRingbufferReturnItem(xiaozhi_handle.decoder_in_ring_h, to_decode_data_pointer); // 整个数据项处理完后归还原始指针，允许接收端复用这块环形缓冲空间。
    }
}