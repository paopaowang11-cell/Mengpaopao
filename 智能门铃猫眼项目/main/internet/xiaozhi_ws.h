/**
 * @file xiaozhi_ws.h
 * WSS 通信接口：客户端初始化、按需连接、控制消息与音频发送。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once
#include "stdbool.h"
#include "stddef.h"

/**
 * @brief 根据激活响应创建 WSS 客户端并注册事件，不在此处发起连接。
 * 使用证书包校验 TLS，并添加 Authorization、Protocol-Version、Device-Id、Client-Id。
 * 自动重连关闭，由后续唤醒流程调用 open_channel 主动建立连接。
 */
void xiaozhi_ws_init(void);

/**
 * @brief 发送 hello，声明协议版本、传输方式和 16 kHz/单声道/60 ms 的 Opus 参数。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_hello(void);
/**
 * @brief 确认 WebSocket 连接可用，必要时启动客户端并等待连接事件。
 * @return 已连接或在 15 秒内收到连接事件时返回 true；启动失败/等待超时返回 false。
 * 等待时间使用 pdMS_TO_TICKS(15000)，与配置项 network_timeout_ms 的 10 秒分别生效。
 * 调用会阻塞，因此由独立唤醒任务执行。
 */
bool xiaozhi_ws_open_channel(void);
/**
 * @brief 上报已检测到唤醒词，发送模板中的 listen/detect 和唤醒词文本。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_detect(void);

/**
 * @brief 请求服务器中断当前回答；此封装只发送控制消息。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_abort(void);

/**
 * @brief 通知服务器当前一轮监听结束，发送 listen/stop 模板。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_stop_listen(void);
/**
 * @brief 通知服务器开始一轮监听，发送 listen/start 模板。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_start_listen(void);

/**
 * @brief 将一段 Opus 音频作为 WebSocket 二进制消息发送。
 * @param data 调用期间保持有效的音频数据地址。
 * @param len 音频有效字节数，数据可能含零字节，因此不能使用 strlen。
 * 底层等待参数为 2000 tick，当前 100 Hz 配置下约 20 秒。
 */
void xiaozhi_ws_send_binary(char *data, size_t len);

/**
 * @brief 上报 Speaker 的属性与方法描述，供服务器生成音量/静音控制命令。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_iot_desc(void);
/**
 * @brief 发送预置的 Speaker 状态模板；当前内容为固定值，不读取硬件实时状态。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_iot_state(void);