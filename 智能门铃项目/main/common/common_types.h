/**
 * @file common_types.h
 * 全项目共享的业务类型、句柄与事件位定义。
 * xiaozhi_handle 的唯一实体在 main.c，其他模块通过 extern 访问。
 * 这些字段由多个任务协作读写；结构体本身不提供互斥或原子访问保障。
 */
#pragma once
#include <stdbool.h>

#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include "freertos/ringbuf.h"
#include "freertos/event_groups.h"

typedef enum
{
    XIAOZHI_STATUS_IDLE,      // 小智啥也没干 既没说话也没监听
    XIAOZHI_STATUS_LISTENING, // 小智正在听我们说
    XIAOZHI_STATUS_SPEAKING,  // 小智正在说话
} xiaozhi_status_enum;

typedef struct
{
    // 激活响应中复制的 WSS 地址，供 WebSocket 客户端初始化使用。
    char *websocket_url;
    // 激活响应中复制的令牌，用于生成 Authorization: Bearer 请求头。
    char *websocket_token;
    // true 表示按激活接口响应判断为已激活；false 时界面提示激活码。
    bool active_flag;  // 激活标志
    // 未激活设备使用的堆内存字符串，当前分配 7 字节保存 6 位码及结束符。
    char *active_code; // 用来保存激活码
    // 指向激活模块常驻的 MAC 文本数组，作为协议中的 Device-Id。
    char *mac_str;
    // 指向激活模块常驻的 UUID 数组；从 NVS 恢复，作为协议中的 Client-Id。
    char *uuid; // 这个也叫client_id

    // 整个业务中所有的环形缓冲区句柄
    // 8 KiB BYTEBUF：AFE 生产、Opus 编码任务消费，内容是 16 kHz/16 bit 单声道 PCM。
    RingbufHandle_t sr2encoder_ringbuffer_handle;
    // 16 KiB NOSPLIT：编码任务生产，发送任务及当前调试任务竞争消费 Opus 数据项。
    RingbufHandle_t encoder_out_ring_h;
    // 16 KiB NOSPLIT：WebSocket 接收回调生产、解码播放任务消费 Opus 数据项。
    RingbufHandle_t decoder_in_ring_h;

    // 收到服务器 hello 后置 true，开启 VAD 业务与音频转发；WS 错误/结束时清零。
    bool wake_flag;
    // 上一轮已记录的人声状态，用于判断 VAD 是否发生变化。
    vad_state_t last_vad_state;
    // 本轮人声状态，AFE 取数任务更新，VAD 回调读取。
    vad_state_t current_vad_state;

    // 唤醒词回调，在 AFE 取数任务上下文调用；本项目只负责通知唤醒工作任务。
    void (*wake_callback)(void);
    // 人声/静音切换回调，在 AFE 取数任务上下文调用。
    void (*vad_changed_callback)(void);

    // 文本回调：data 为临时帧数据，data_len 为字节数，不保证有字符串结束符。
    void (*ws_textframe_process)(char *data, int data_len);

    // 二进制回调：将回调期内有效的 Opus 数据复制到解码输入缓冲区。
    void (*ws_binaryframe_process)(char *data, int data_len);

    // ws相关的业务逻辑的变量
    // 全局业务事件组，连接事件和 hello 应答事件由不同位表示。
    EventGroupHandle_t event_group_h;

    // 会话业务状态：空闲/监听/说话，不等同于 Wi-Fi 或 WebSocket 的连接状态。
    xiaozhi_status_enum current_state;
} xiaozhi_handle_t;

// BIT0：WebSocket 已连接，供 open_channel 等待；断开时清除。
#define XIAOZHI_EVENT_WS_CONNECTED BIT0
// BIT1：收到服务器 hello；唤醒流程等待成功后自动清除。
#define XIAOZHI_EVENT_RECEIVE_HELLO BIT1
typedef struct
{
    // 服务器返回的英文表情标识，用于匹配 llm 消息的 emotion。
    char *name; // 协议侧表情名称。
    // 对应的 UTF-8 表情字符串，由表情字库绘制。
    char *emoji;
} xiaozhi_emoji_t;

extern xiaozhi_handle_t xiaozhi_handle;