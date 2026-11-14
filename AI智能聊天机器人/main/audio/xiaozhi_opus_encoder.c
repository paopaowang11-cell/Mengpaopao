/**
 * @file xiaozhi_opus_encoder.c
 * 上行音频压缩模块，把 16 bit PCM 字节流组装为 60 ms 帧并编码为 Opus。
 * 工作缓冲区和任务栈使用 PSRAM，常驻任务通过环形缓冲区与前后级解耦。
 */
#include "xiaozhi_opus_encoder.h"
#include "esp_opus_enc.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "freertos/ringbuf.h"
#include "common/common_types.h"
#include <string.h>

#define TAG "xiaozhi_opus_encoder"

// 工作区由编码任务独占复用；输入为 PCM，输出为压缩后的 Opus 包。
uint8_t *pcm_data_buffer;
uint8_t *opus_data_buffer;
int pcm_input_size = 0;
int opus_output_size = 0;
esp_audio_enc_handle_t encoder = NULL;

void xiaozhi_opus_encoder_task(void *arg);
/**
 * @brief 创建 16 kHz 单声道 Opus 编码器、输入输出工作区及编码任务。
 * 参数：16 bit PCM、60 ms/帧、32 kbit/s、复杂度 5，关闭 FEC、DTX 和 VBR。
 * PCM 输入每帧为 16000 × 0.060 × 1 × 2 = 1920 字节，缓冲区尺寸以库查询结果为准。
 */
void xiaozhi_opus_encoder_init(void)
{

    esp_opus_enc_config_t opus_cfg = { // 定义 Opus 编码参数，后续 open 使用这份结构创建编码器。
        .sample_rate = ESP_AUDIO_SAMPLE_RATE_16K, // 输入 PCM 的采样率为 16 kHz。
        .channel = ESP_AUDIO_MONO, // 输入音频为单声道，与麦克风和 AFE 输出对应。
        .bits_per_sample = ESP_AUDIO_BIT16, // 每个 PCM 采样值占 16 bit，即 2 字节。
        // 目标码率单位为 bit/s；32 kbit/s × 60 ms 名义负载约 240 字节，容量仍以库查询为准。
        .bitrate = 32000, // 设置目标编码码率为每秒 32000 bit，即 32 kbit/s。
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS, // 每个编码帧覆盖 60 ms，对应 1920 字节单声道 16 bit PCM。
        .application_mode = ESP_OPUS_ENC_APPLICATION_AUDIO, // 采用 AUDIO 应用模式，具体编码策略交由 Opus 库实现。
        .complexity = 5, // 设置复杂度为 5，在计算量与压缩质量之间选择此档位。
        // FEC=前向纠错，DTX=静音不连续发送，VBR=可变码率；当前三项均关闭。
        .enable_fec = false, // 关闭前向纠错 FEC，不附带该项抗丢包冗余数据。
        .enable_dtx = false, // 关闭 DTX，不由编码器启用静音不连续发送。
        .enable_vbr = false, // 关闭可变码率 VBR，按当前固定码率配置编码。
    };

    esp_opus_enc_open(&opus_cfg, sizeof(esp_opus_enc_config_t), &encoder); // 传入配置及结构体大小创建编码器，并把句柄写入 encoder。

    // 分别查询单帧 PCM 输入长度和 Opus 输出工作区所需容量，单位均为字节。
    esp_opus_enc_get_frame_size(encoder, &pcm_input_size, &opus_output_size); // 查询一帧需要的 PCM 输入字节数和输出工作缓冲区容量。
    ESP_LOGI(TAG, "pcm_input_size:%d, opus_output_size:%d", pcm_input_size, opus_output_size); // 打印库返回的两种帧尺寸，方便核对配置和内存分配。
    // 创建encoder的两个小缓冲区
    pcm_data_buffer = heap_caps_malloc(pcm_input_size, MALLOC_CAP_SPIRAM); // 从 PSRAM 申请一帧 PCM 工作区，用于拼接 AFE 输出字节流。
    opus_data_buffer = heap_caps_malloc(opus_output_size, MALLOC_CAP_SPIRAM); // 从 PSRAM 申请 Opus 输出工作区，容量以编码库查询结果为准。

    // 编码任务固定到核心 1，优先级 5，PSRAM 栈大小为 32 KiB。
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_opus_encoder_task, "xiaozhi_opus_encoder_task", 32 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM); // 创建编码任务：32 KiB PSRAM 栈、优先级 5、核心 1，不保存任务句柄。
}

// 不停地从环形缓冲区拉数据来进行编码
/**
 * @brief 从 AFE 字节流拼出完整 PCM 帧，编码为 Opus 数据项。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * BYTEBUF 每次可能只返回部分字节，因此循环拷贝直到填满一帧，再调用编码器。
 * 输出只发送 encoded_bytes 个有效字节；out_frame.len 表示容量而不是编码后的长度。
 */
void xiaozhi_opus_encoder_task(void *arg)
{
    esp_audio_enc_in_frame_t in_frame = { // 定义输入帧描述，把后续拼满的 PCM 工作区交给编码器。
        .buffer = pcm_data_buffer, // 输入数据地址指向初始化时分配的 PCM 缓冲区。
        .len = pcm_input_size, // 输入长度设为一整帧的 PCM 字节数，当前配置下为 1920。
    };

    esp_audio_enc_out_frame_t out_frame = { // 定义输出帧描述，提供编码器写入 Opus 数据的位置与容量。
        .buffer = opus_data_buffer, // 输出地址指向预先分配的 Opus 工作缓冲区。
        .len = opus_output_size, // 填写输出容量；真正压缩后的有效长度会写入 encoded_bytes。
    };
    while (1) // 不断重复“收齐 PCM 一帧、压缩、写入输出缓冲”的过程。
    {
        size_t remain_frame_size = pcm_input_size; // 每轮开始都重置尚缺的 PCM 字节数，初值是一整帧长度。
        uint8_t *current_buffer_start_position = in_frame.buffer; // 写入游标回到 PCM 缓冲区开头，后续随已拷贝数据向后移动。
        while (remain_frame_size > 0) // 一帧尚未填满时继续取数，不能用不足一帧的数据直接编码。
        {
            // 从环形缓冲区里 1920字节 1920字节地读
            size_t this_time_read_size = 0; // 准备接收本次实际读到的字节数，可能小于尚缺长度。
            // 只要环形缓冲区里有数据 哪怕不满1920 这个函数也会结束
            void *data_pointer = xRingbufferReceiveUpTo(xiaozhi_handle.sr2encoder_ringbuffer_handle, &this_time_read_size, portMAX_DELAY, remain_frame_size); // 最多取 remain_frame_size 字节；无数据则阻塞，有部分数据即可返回。
            // 内存拷贝 把环形缓冲区里的数据拷贝in_frame里
            memcpy(current_buffer_start_position, data_pointer, this_time_read_size); // 把本次借出的音频字节复制到当前写入游标，不覆盖此前已拼好的内容。
            // 取消对环形缓冲区的占用
            vRingbufferReturnItem(xiaozhi_handle.sr2encoder_ringbuffer_handle, data_pointer); // 复制后立即归还本次数据项，释放对环形缓冲区的占用。
            // 向后移动指针，避免每次都把数据copy到in_frame开始位置
            current_buffer_start_position += this_time_read_size; // 游标前进本次拷贝的字节数，使下一段紧接上一段存放。
            // 计算这个60ms帧 还剩多少字节
            remain_frame_size -= this_time_read_size; // 扣除已拷贝字节数；减到 0 时说明一整帧已收齐。
        }

        // 经过上面的循环 一帧已经读满了 开始做压缩这个事
        esp_opus_enc_process(encoder, &in_frame, &out_frame); // 将完整 PCM 输入帧压缩到 Opus 输出区，并由库填写编码结果长度。
        // 发送给下一个环形缓冲区
        xRingbufferSend(xiaozhi_handle.encoder_out_ring_h, out_frame.buffer, out_frame.encoded_bytes, portMAX_DELAY); // 只把 encoded_bytes 个有效字节复制到编码输出缓冲；满时阻塞等待空间。
    }
}
