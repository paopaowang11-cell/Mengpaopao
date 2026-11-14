/**
 * @file xiaozhi_ws.c
 * WebSocket 协议适配层：WSS 连接、事件分派、控制 JSON 与音频发送。
 * 文本控制与二进制音频共用连接；具体业务处理回调由 main.c 注册。
 */
#include "internet/xiaozhi_ws.h"

#include "esp_websocket_client.h"
#include "common/common_types.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "cJSON.h"
#include "display/xiaozhi_display.h"
#define TAG "xiaozhi_ws"

// 这些 .txt 文件实际是带双引号的 C 字符串片段，经预处理直接组成变量初始化值。
char *hello_json_str =
#include "pre_data/xiaozhi_hello.txt"
    ;
char *detect_json_str =
#include "pre_data/xiaozhi_detect.txt"
    ;

char *start_listen_json_str =
#include "pre_data/xiaozhi_start_listen.txt"
    ;

char *stop_listen_json_str =
#include "pre_data/xiaozhi_stop_listen.txt"
    ;

char *abort_json_str =
#include "pre_data/xiaozhi_abort.txt"
    ;

char *iot_desc_json_str =
#include "pre_data/xiaozhi_iot_desc.txt"
    ;

char *iot_state_json_str =
#include "pre_data/xiaozhi_iot_state.txt"
    ;

// 客户端在 init 时创建，首次唤醒时才 start；控制消息和音频共用此连接。
esp_websocket_client_handle_t ws_client;
/**
 * @brief 将 WebSocket 连接与接收事件转换成业务事件和音频/文本回调。
 * @param handler_args 注册时传入的客户端句柄，本回调未直接使用。
 * @param base WebSocket 事件源。
 * @param event_id 连接、断开、收数据、错误或结束事件编号。
 * @param event_data 组件维护的事件数据，数据指针仅在回调期间有效。
 * 0x01 按文本分派，0x02 按二进制分派；当前直接处理每次事件的数据，未重组分片。
 * 错误或结束时清除已唤醒状态；单纯断开事件只清除连接事件位。
 */
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data; // 将通用事件数据转换成 WebSocket 专用结构，读取操作码、数据地址及长度。
    switch (event_id) // 根据 WebSocket 事件编号选择连接、收数据或错误处理分支。
    {
    case WEBSOCKET_EVENT_BEGIN: // 客户端开始运行时进入 BEGIN 分支。
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN"); // 打印客户端开始运行日志，当前不在此阶段置连接完成位。
        break; // 结束 BEGIN 事件处理。
    case WEBSOCKET_EVENT_CONNECTED: // 与服务器完成 WebSocket 连接后进入此分支。
        // 给标志位置位
        xEventGroupSetBits(xiaozhi_handle.event_group_h, XIAOZHI_EVENT_WS_CONNECTED); // 置位连接事件，解除 open_channel 中对通道就绪的等待。
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED"); // 打印 WebSocket 已连接日志。
        break; // 结束连接事件分支，继续等待数据或后续连接变化。
    case WEBSOCKET_EVENT_DISCONNECTED: // 连接断开时进入此分支。
        xEventGroupClearBits(xiaozhi_handle.event_group_h, XIAOZHI_EVENT_WS_CONNECTED); // 清除全局事件组的连接位，表示通道已经不可用。
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED"); // 记录 WebSocket 断开，当前分支本身不清 wake_flag。
        break; // 结束断开事件处理。
    case WEBSOCKET_EVENT_DATA: // 收到协议数据事件时进入此分支，数据可能是文本或二进制。
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA"); // 打印收到 WebSocket 数据的事件日志。
        ESP_LOGI(TAG, "Received opcode=%d", data->op_code); // 打印操作码，辅助区分文本、二进制及其他帧类型。
        // 文本帧(这里面有服务器返回的字幕、表情等信息)
        if (data->op_code == 0x01) // 操作码 0x01 表示文本数据，按控制消息路径处理。
        {
            ESP_LOGI(TAG, "Received text: %.*s", data->data_len, data->data_ptr); // %.*s 用 data_len 限制读取长度，避免日志依赖接收区末尾有空字符。
            // 处理文本数据
            xiaozhi_handle.ws_textframe_process(data->data_ptr, data->data_len); // 将当前数据地址及长度传给主程序文本回调，解析字幕、表情和控制命令。
        }
        // 二进制帧(这里面有服务器的音频数据)
        if (data->op_code == 0x02) // 操作码 0x02 表示二进制数据，本项目按下行 Opus 音频处理。
        {
            ESP_LOGI(TAG, "Received binary frame"); // 记录收到二进制音频帧的日志。
            xiaozhi_handle.ws_binaryframe_process(data->data_ptr, data->data_len); // 将音频地址及有效字节数传给业务回调，由它复制到解码输入缓冲。
        }
        break; // 结束本次数据事件处理，其他操作码在当前实现中不另外分派。
    case WEBSOCKET_EVENT_ERROR: // 客户端报告错误时进入此分支。
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR"); // 记录错误事件，便于观察会话异常中止。
        xiaozhi_handle.current_state = XIAOZHI_STATUS_IDLE; // 把业务状态恢复为空闲，避免继续保留监听或说话状态。
        xiaozhi_handle.wake_flag = false; // 取消已经唤醒标志，后续需要重新唤醒并完成握手。
        break; // 结束错误事件处理。
    case WEBSOCKET_EVENT_FINISH: // 客户端运行结束时进入 FINISH 分支。
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH"); // 记录客户端结束日志，与临时断开事件区分。
        xiaozhi_handle.current_state = XIAOZHI_STATUS_IDLE; // 将会话状态重置为空闲。
        xiaozhi_handle.wake_flag = false; // 清除唤醒标志，停止后续 AFE 人声业务转发。
        break; // 结束 FINISH 事件处理。
    }
}
/**
 * @brief 根据激活响应创建 WSS 客户端并注册事件，不在此处发起连接。
 * 使用证书包校验 TLS，并添加 Authorization、Protocol-Version、Device-Id、Client-Id。
 * 自动重连关闭，由后续唤醒流程调用 open_channel 主动建立连接。
 */
void xiaozhi_ws_init(void)
{
    esp_websocket_client_config_t websocket_cfg = {}; // 清零 WebSocket 客户端配置，未指定项沿用组件的默认处理。
    websocket_cfg.uri = xiaozhi_handle.websocket_url; // 把激活响应保存的 URL 设为客户端目标地址。
    // 1. 跟wss相关
    websocket_cfg.transport = WEBSOCKET_TRANSPORT_OVER_SSL; // 强制使用 SSL 传输，建立 WSS 加密连接。
    websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach; // 附加 ESP 证书包，供 TLS 校验服务器证书信任链。
    // 关闭自动重连，让每次唤醒按业务流程显式建立通道。
    websocket_cfg.disable_auto_reconnect = true; // 禁用组件自动重连，由主业务在唤醒时显式启动通道。
    websocket_cfg.network_timeout_ms = 10000; // 设置网络操作超时为 10000 ms，区别于稍后等待连接事件的 15 秒。

    // 完成配置后直接初始化
    ws_client = esp_websocket_client_init(&websocket_cfg); // 按配置创建客户端句柄；此时未调用 start，因此尚不发起连接。

    // 根据websocket_token的实际长度分配刚好够用的堆空间 然后在这个空间中生成目标字符串
    uint32_t auth_header_str_len = strlen("Bearer ") + strlen(xiaozhi_handle.websocket_token); // 计算 Bearer 前缀（含空格）与令牌的总字符数，不含结束符。
    char *auth_header_str = heap_caps_malloc(auth_header_str_len + 1, MALLOC_CAP_SPIRAM); // 从 PSRAM 多申请 1 字节，保存完整认证头值及字符串结束符。
    auth_header_str[auth_header_str_len] = '\0'; // 预先把最后一个位置设为空字符，明确字符串容量边界。
    sprintf(auth_header_str, "Bearer %s", xiaozhi_handle.websocket_token); // 把 Bearer 前缀和激活令牌拼成 Authorization 头字段的值。

    // Bearer 后必须保留空格，后接激活接口返回的访问令牌。
    esp_websocket_client_append_header(ws_client, "Authorization", auth_header_str); // 向 WebSocket 握手请求追加认证头，使用上面生成的 Bearer 字符串。
    esp_websocket_client_append_header(ws_client, "Protocol-Version", "1"); // 声明业务协议版本为 1，使服务器按对应协议处理消息。
    esp_websocket_client_append_header(ws_client, "Device-Id", xiaozhi_handle.mac_str); // 将 STA MAC 文本作为 Device-Id，沿用激活查询使用的设备身份。
    esp_websocket_client_append_header(ws_client, "Client-Id", xiaozhi_handle.uuid); // 将持久化 UUID 作为 Client-Id，保持连接与激活查询的客户端标识一致。

    // 注册事件回调
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)ws_client); // 注册所有 WebSocket 事件到统一回调，并把客户端句柄作为用户参数传入。
}
/**
 * @brief 确认 WebSocket 连接可用，必要时启动客户端并等待连接事件。
 * @return 已连接或在 15 秒内收到连接事件时返回 true；启动失败/等待超时返回 false。
 * 等待时间使用 pdMS_TO_TICKS(15000)，与配置项 network_timeout_ms 的 10 秒分别生效。
 * 调用会阻塞，因此由独立唤醒任务执行。
 */
bool xiaozhi_ws_open_channel(void)
{
    // 如果已经连接上了 就直接返回true
    if (esp_websocket_client_is_connected(ws_client)) // 先查询客户端是否已经连上，避免对已有连接重复启动。
    {
        return true; // 已有连接即可直接返回 true，继续使用同一个通道。
    }
    else // 尚未连接时才执行清事件、启动客户端及等待连接的流程。
    {
        // 如果没有连上就尝试调用start进行连接
        xEventGroupClearBits(xiaozhi_handle.event_group_h, XIAOZHI_EVENT_WS_CONNECTED); // 清除可能残留的旧连接位，确保本次等待反映新连接事件。
        if (esp_websocket_client_start(ws_client) != ESP_OK) // 启动客户端并检查返回码，start 成功只代表启动请求被接受。
        {
            ESP_LOGE(TAG, "Failed to start WebSocket client"); // 启动失败时打印错误，帮助定位客户端状态或资源问题。
            return false; // 返回 false 告诉唤醒流程本次未建立可用通道。
        }
        // 等待一个事件标志位
        EventBits_t bits = xEventGroupWaitBits(xiaozhi_handle.event_group_h, XIAOZHI_EVENT_WS_CONNECTED, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000)); // 等待连接位最多 15 秒；返回清位，不要求多个事件同时到达，毫秒通过宏转换为 tick。
        if (!(bits & XIAOZHI_EVENT_WS_CONNECTED)) // 按位检查连接标志，未置位意味着本次等待没有获得连接成功通知。
        {
            // 15 秒内未等到连接事件，停止客户端并显示超时提示。
            esp_websocket_client_stop(ws_client); // 停止本次未成功连接的客户端运行，避免超时后继续维持这轮连接尝试。
            xiaozhi_display_text("连接超时，请重试"); // 在字幕区域提示连接超时，告知用户可以再试。
            xiaozhi_display_emoji("crying"); // 将表情更新为 crying，与连接失败提示配合。
            return false; // 返回 false，阻止主业务继续发送 hello 等控制消息。
        }
        // 收到连接事件后返回成功，后续由业务层继续 hello 握手。
        return true; // 收到本轮连接事件后返回 true，由上层继续执行协议握手。
    }
}

/**
 * @brief 将以空字符结尾的控制 JSON 作为 WebSocket 文本发送。
 * @param data 待发送字符串；strlen 计算长度，不发送结尾空字符。
 * 底层等待参数为 2000 tick，当前 100 Hz 配置下约 20 秒；此封装不返回发送结果。
 */
void xiaozhi_ws_send_txt(char *data)
{
    esp_websocket_client_send_text(ws_client, data, strlen(data), 2000); // 按 strlen 得到 JSON 字节数发送文本；等待参数 2000 tick，当前约 20 秒，不包含结束符。
}

/**
 * @brief 将一段 Opus 音频作为 WebSocket 二进制消息发送。
 * @param data 调用期间保持有效的音频数据地址。
 * @param len 音频有效字节数，数据可能含零字节，因此不能使用 strlen。
 * 底层等待参数为 2000 tick，当前 100 Hz 配置下约 20 秒。
 */
void xiaozhi_ws_send_binary(char *data, size_t len)
{
    esp_websocket_client_send_bin(ws_client, data, len, 2000); // 按显式 len 发送二进制音频，允许数据中含零字节；发送等待上限为 2000 tick。
}

/**
 * @brief 发送 hello，声明协议版本、传输方式和 16 kHz/单声道/60 ms 的 Opus 参数。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_hello(void)
{
    xiaozhi_ws_send_txt(hello_json_str); // 发送 hello 预置 JSON，声明协议、传输方式及 Opus 音频参数。
}

/**
 * @brief 上报已检测到唤醒词，发送模板中的 listen/detect 和唤醒词文本。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_detect(void)
{
    xiaozhi_ws_send_txt(detect_json_str); // 发送 listen/detect 预置 JSON，告诉服务器本机已检测到指定唤醒词。
}

/**
 * @brief 通知服务器开始一轮监听，发送 listen/start 模板。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_start_listen(void)
{
    xiaozhi_ws_send_txt(start_listen_json_str); // 发送 listen/start 预置 JSON，请服务器开始接收本轮用户语音输入。
}

/**
 * @brief 通知服务器当前一轮监听结束，发送 listen/stop 模板。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_stop_listen(void)
{
    xiaozhi_ws_send_txt(stop_listen_json_str); // 发送 listen/stop 预置 JSON，通知服务器本轮用户语音输入已经结束。
}

/**
 * @brief 请求服务器中断当前回答；此封装只发送控制消息。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_abort(void)
{
    xiaozhi_ws_send_txt(abort_json_str); // 发送 abort 预置 JSON，请求服务器中断当前回答，不直接清空本地播放缓冲。
}

/**
 * @brief 上报 Speaker 的属性与方法描述，供服务器生成音量/静音控制命令。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_iot_desc(void)
{
    xiaozhi_ws_send_txt(iot_desc_json_str); // 发送 Speaker 能力描述模板，上报音量、静音属性及对应可调用方法。
}

/**
 * @brief 发送预置的 Speaker 状态模板；当前内容为固定值，不读取硬件实时状态。
 * 消息内容来自 pre_data 下的同名 C 字符串模板。
 */
void xiaozhi_ws_send_iot_state(void)
{
    xiaozhi_ws_send_txt(iot_state_json_str); // 发送当前固定的 Speaker 状态模板，内容不从 ES8311 实时查询。
}
