/**
 * @file main.c
 * 应用业务入口与会话状态机。
 * 上行：麦克风 → AFE/VAD → PCM 环形缓冲区 → Opus 编码 → WebSocket。
 * 下行：WebSocket → Opus 环形缓冲区 → 解码 → ES8311 扬声器。
 * 网络握手由独立唤醒任务处理，界面和协议控制由回调协同驱动。
 */
#include <stdio.h>
#include "internet/xiaozhi_wifi.h"
#include "esp_log.h"
#include "button/xiaozhi_button.h"
#include "display/xiaozhi_display.h"
#include "internet/xiaozhi_active.h"
#include "common/common_types.h"
#include "esp_system.h"
#include "audio/xiaozhi_sr.h"
#include "audio/xiaozhi_opus_encoder.h"
#include "audio/xiaozhi_opus_decoder.h"
#include "internet/xiaozhi_ws.h"
#include "audio/xiaozhi_hard_codec.h"
#define TAG "main"
// 全局静态存储期对象，上电时成员自动清零；后续由初始化流程填写各资源句柄。
xiaozhi_handle_t xiaozhi_handle;
static TaskHandle_t wake_task_handle;
// 双核自旋锁只保护 wake_pending；临界区内不执行网络等待或其他耗时操作。
static portMUX_TYPE wake_lock = portMUX_INITIALIZER_UNLOCKED;
static bool wake_pending;

/**
 * @brief 执行一次唤醒业务；由独立 wake_task 调用，允许等待网络事件。
 * 空闲时先建立 WebSocket，再发送 hello、设备能力和状态，等待服务器 hello 后允许语音上传。
 * 说话时再次唤醒只发送 abort 请求；停止播放和状态切换仍由后续服务器消息决定。
 * hello 等待参数 5000 的单位是 FreeRTOS tick；当前 100 Hz 配置下最长约 50 秒。
 */
static void handle_wake(void)
{
    if (xiaozhi_handle.current_state == XIAOZHI_STATUS_IDLE) // 只有当前处于空闲状态，才走建立通道并发送唤醒消息的流程。
    {
        bool connected = xiaozhi_ws_open_channel(); // 确认或建立 WebSocket 连接，返回值表示通道是否就绪。
        if (connected)                              // 仅在连接成功时继续发送握手及设备能力消息。
        {
            // 先清除上一轮 hello 事件，避免把残留应答误判为本轮握手成功。
            xEventGroupClearBits(xiaozhi_handle.event_group_h, XIAOZHI_EVENT_RECEIVE_HELLO); // 清除上一次 hello 应答事件，避免旧事件使本轮等待立即误通过。
            xiaozhi_ws_send_hello();                                                         // 发送 hello 模板，向服务器声明本机协议版本和音频格式。
            xiaozhi_ws_send_iot_desc();                                                      // 上报 Speaker 设备能力，例如设置音量和静音的方法。
            xiaozhi_ws_send_iot_state();                                                     // 发送预置 Speaker 状态模板，当前模板不是硬件状态实时快照。
            // 等到服务器回hello
            EventBits_t bits = xEventGroupWaitBits(xiaozhi_handle.event_group_h, XIAOZHI_EVENT_RECEIVE_HELLO, true, true, 5000); // 等待 hello 事件；两个 true 分别表示退出清位、等待全部指定事件，5000 为 tick。
            if (bits & XIAOZHI_EVENT_RECEIVE_HELLO)                                                                              // 对等待返回值做按位与，确认返回原因确实包含 hello 应答而非单纯超时。
            {
                xiaozhi_handle.wake_flag = true; // 标记会话已经唤醒，允许 AFE 转发人声音频并触发 VAD 业务。
                xiaozhi_ws_send_detect();        // 发送 listen/detect，把已检测到唤醒词的信息通知服务器。
            }
            else // 连接虽成功但没有等到 hello 时，进入应答超时处理。
            {
                xiaozhi_display_text("服务器没礼貌呢?"); // 在屏幕字幕区域显示现有的握手失败提示。
                return;                                  // 结束本次唤醒处理，不继续上报唤醒词。
            }
            // 等待事件标志位
            // 发送唤醒词
        }
    }
    else if (xiaozhi_handle.current_state == XIAOZHI_STATUS_SPEAKING) // 若当前正在播放服务器回答，则把再次唤醒理解为打断请求。
    {
        xiaozhi_ws_send_abort(); // 发送 abort 控制消息，请求服务器中断回答。
    }
}

/**
 * @brief 唤醒工作任务：等待任务通知，把网络连接和握手等待移出 AFE 取数任务。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * 处理结束后在临界区清除 wake_pending，允许下一次唤醒请求；任务常驻运行。
 */
static void wake_task(void *arg)
{
    while (1) // 常驻等待唤醒通知，每轮只执行一次完整的唤醒业务。
    {
        // 没有唤醒请求时阻塞，不占用 CPU；pdTRUE 表示唤醒时清空累计通知计数。
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // 无限等待任务通知；pdTRUE 在取得通知时清空计数，没有请求时不忙轮询。
        handle_wake();                           // 在此工作任务内执行网络连接、握手等待或打断请求。
        portENTER_CRITICAL(&wake_lock);          // 进入双核临界区，保护即将更新的 wake_pending 标志。
        wake_pending = false;                    // 本轮处理完成，清除待处理标志，允许后续唤醒再次通知。
        portEXIT_CRITICAL(&wake_lock);           // 退出临界区，释放自旋锁，恢复其他上下文对该标志的访问。
    }
}

/**
 * @brief AFE 检测到唤醒词时的任务上下文回调。
 * 通过短临界区合并处理期间重复到达的唤醒请求，再通知 wake_task 执行业务。
 * 使用普通任务通知 API，并非中断回调；首次触发前须创建有效的 wake_task_handle。
 */
void wake_cb(void)
{
    portENTER_CRITICAL(&wake_lock); // 进入临界区，避免两个核心同时判断和更新待处理标志。
    bool start = !wake_pending;     // 当没有唤醒请求正在处理时，才把 start 置真。
    if (start)
        wake_pending = true;       // 仅对首次请求设置 pending，处理期间的重复唤醒被合并。
    portEXIT_CRITICAL(&wake_lock); // 完成标志更新后立即退出临界区，不在锁内执行通知或网络操作。
    if (start)
        xTaskNotifyGive(wake_task_handle); // 确需启动处理时给 wake_task 增加通知计数，使其从等待中唤醒。
}

/**
 * @brief 根据服务器 JSON 文本中的 type 分派控制消息。
 * @param data WebSocket 回调提供的临时数据地址，不要求以空字符结尾。
 * @param data_len 本次数据的有效字节数，传给带长度的 cJSON 解析接口。
 * hello 通知握手完成；tts 更新字幕和说话状态；llm 更新表情；iot 控制音量或静音。
 * 当前按服务器字段齐全的协议消息处理，IoT commands 数组只执行第一个命令。
 */
void ws_textframe_process(char *data, int data_len)
{
    // WebSocket 数据可能没有字符串结束符，因此按实际长度解析。
    cJSON *root = cJSON_ParseWithLength(data, data_len); // 按 data_len 字节解析 JSON，不依赖 WebSocket 数据尾部存在空字符。
    if (root == NULL)                                    // 检查解析是否失败；失败时不能继续访问 JSON 节点。
    {
        ESP_LOGW("main", "woc 服务器不讲json"); // 记录服务端数据无法解析为 JSON 的警告。
        return;                                 // 立即结束本次文本处理，此时未创建有效 JSON 树。
    }
    cJSON *type = cJSON_GetObjectItem(root, "type"); // 从根对象读取 type 字段，用来区分 hello、tts、llm、iot 等消息。
    if (type == NULL)                                // 检查消息是否缺少 type 字段，避免对空节点取 valuestring。
    {
        ESP_LOGW("main", "woc 缺少type"); // 打印缺少消息类型的警告，便于定位协议异常。
        return;                           // 结束本次处理；当前这条提前返回路径没有释放已解析的 root。
    }
    // 处理服务器发过来的hello json
    if (strcmp(type->valuestring, "hello") == 0) // strcmp 返回 0 表示类型为 hello，即服务器握手应答。
    {
        xEventGroupSetBits(xiaozhi_handle.event_group_h, XIAOZHI_EVENT_RECEIVE_HELLO); // 置位 hello 事件，唤醒正在 handle_wake 中等待应答的工作任务。
    }
    // 处理服务器发来的句子字幕
    if (strcmp(type->valuestring, "tts") == 0) // 类型为 tts 时，处理服务器语音合成生命周期及字幕消息。
    {
        cJSON *tts_state = cJSON_GetObjectItem(root, "state");     // 读取 tts 消息的 state 字段，进一步区分开始、结束及句子开始。
        if (strcmp(tts_state->valuestring, "sentence_start") == 0) // sentence_start 表示服务器开始播报新句子，此时按协议取出字幕。
        {
            // 说明这是一个句子的开始 然后text里面一定有字幕可以显示
            cJSON *text = cJSON_GetObjectItem(root, "text"); // 读取当前句子的 text 文本节点。
            // 在屏幕上显示字幕
            xiaozhi_display_text(text->valuestring); // 把字幕字符串交给 LVGL 显示接口，接口内部会复制文本内容。
        }
        // 判断是不是 tts start
        if (strcmp(tts_state->valuestring, "start") == 0) // state 为 start 时，说明服务器开始输出语音回答。
        {
            xiaozhi_handle.current_state = XIAOZHI_STATUS_SPEAKING; // 把业务状态切到说话，阻止 VAD 将回放声音当成新的监听开始。
        }

        // 判断是不是 tts stop
        if (strcmp(tts_state->valuestring, "stop") == 0) // state 为 stop 时，说明服务器已通知本轮语音回答结束。
        {
            xiaozhi_handle.current_state = XIAOZHI_STATUS_IDLE; // 回到空闲状态；此处依据协议事件切换，不检查本地播放缓冲是否排空。
        }
    }
    // 处理服务器发来的表情
    if (strcmp(type->valuestring, "llm") == 0) // 类型为 llm 时，处理对话模型返回的表情信息。
    {
        cJSON *emotion = cJSON_GetObjectItem(root, "emotion"); // 从 emotion 字段取得英文表情名称节点。
        xiaozhi_display_emoji(emotion->valuestring);           // 把表情名称映射成对应 emoji，在屏幕表情区域显示。
    }

    if (strcmp(type->valuestring, "iot") == 0) // 类型为 iot 时，处理服务器发来的设备控制命令。
    {
        cJSON *cmdsItem = cJSON_GetObjectItem(root, "commands"); // 从 commands 字段取得命令数组。
        // 当前只取 commands[0]；服务端消息需满足下述字段和类型约定。
        cJSON *cmdItem = cJSON_GetArrayItem(cmdsItem, 0);                   // 只取数组下标 0 的命令，当前实现没有遍历其余命令。
        cJSON *parametersItem = cJSON_GetObjectItem(cmdItem, "parameters"); // 取得该命令的 parameters 参数对象。

        char *method = cJSON_GetObjectItem(cmdItem, "method")->valuestring; // 读取 method 字符串，后续根据方法名选择音量或静音控制。

        if (strcmp(method, "SetVolume") == 0) // 判断当前方法是否为 SetVolume，即设置输出音量。
        {
            int volumne = cJSON_GetObjectItem(parametersItem, "volume")->valueint; // 从参数对象中读取 volume 整数，存到现有变量 volumne。

            xiaozhi_hard_codec_set_volume(volumne); // 把音量值传给硬件音频封装，同时更新用于恢复静音的记忆音量。
        }
        else if (strcmp(method, "SetMute") == 0) // 若不是音量命令，再判断是否为 SetMute 静音控制。
        {
            cJSON *muteItem = cJSON_GetObjectItem(parametersItem, "mute"); // 取得 mute 参数节点，稍后确认它是否确实是布尔类型。
            bool is_mute = false;                                          // 先将本地静音变量设为 false，作为参数类型不匹配时的默认值。
            if (cJSON_IsBool(muteItem))                                    // 只有 mute 是合法 JSON 布尔节点时，才按布尔值读取。
            {
                is_mute = cJSON_IsTrue(muteItem); // 将 JSON 的 true/false 转换成本地 bool 状态。
            }
            xiaozhi_hard_codec_set_mute(is_mute); // 按转换结果设置静音，驱动封装会选择音量 0 或记忆音量。
        }
    }

    // JSON 节点占用堆内存，正常处理结束后删除，避免多轮对话累积内存泄漏。
    // 正常处理路径释放整棵 JSON 树；前面的部分错误返回路径未执行此释放。
    cJSON_Delete(root); // 正常路径末尾删除根节点及其子节点，归还这棵 JSON 树的堆内存。
}

/**
 * @brief 把服务器下发的 Opus 音频复制到解码器输入环形缓冲区。
 * @param data 本次回调中的二进制音频数据，回调结束后不保留该地址。
 * @param data_len 音频数据的字节数。
 * xRingbufferSend 会复制内容；缓冲区满时使用 portMAX_DELAY 等待可用空间。
 */
void ws_binaryframe_process(char *data, int data_len)
{
    xRingbufferSend(xiaozhi_handle.decoder_in_ring_h, data, data_len, portMAX_DELAY); // 把本次 data_len 字节 Opus 数据复制到解码输入缓冲；空间不足时无限等待。
}

// 发送音频任务
/**
 * @brief 从编码器输出缓冲区取出 Opus 包，并在监听状态下发送给服务器。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * 非监听状态也会取出并归还数据项，避免旧音频一直堆积；归还后不能再使用 data。
 */
void ws_send_audio_task(void *arg)
{
    while (1) // 持续消费编码器输出数据，监听状态下把 Opus 包上传到服务器。
    {
        size_t data_size = 0; // 为本次收到的编码包准备有效长度变量，单位为字节。

        void *data = xRingbufferReceive(xiaozhi_handle.encoder_out_ring_h, &data_size, portMAX_DELAY); // 等待一个完整编码数据项，接收函数同时返回地址和实际长度。
        if (xiaozhi_handle.current_state == XIAOZHI_STATUS_LISTENING && data_size > 0)                 // 仅在正在监听且包非空时上传，其他状态下这项数据会被消费后丢弃。
        {
            xiaozhi_ws_send_binary(data, data_size); // 按显式字节长度发送二进制音频，不能用 strlen 计算压缩音频长度。
        }

        // 无论音频是否发送，都要归还数据项，使生产者能够复用缓冲区空间。
        vRingbufferReturnItem(xiaozhi_handle.encoder_out_ring_h, data); // 发送或丢弃后均归还数据项，防止编码输出缓冲区被占满。
    }
}

// 人声检测回调
// 一定不能让扬声器回来的人声触发这个函数里的逻辑
// 之所以成这个样子 是因为缺少回声消除的硬件措施。所以只能手动加代码
/**
 * @brief 仅在人声/静音状态发生变化时调整会话状态并发送监听控制消息。
 * 空闲时检测到人声进入 LISTENING；静音时若未处于 SPEAKING，则回到 IDLE。
 * 说话期间不由 VAD 启动监听，以减少扬声器回放触发麦克风造成的误判。
 * 这是业务状态门控，当前 AFE 配置关闭了声学回声消除（AEC）。
 */
void vad_changed_cb(void)
{
    if (xiaozhi_handle.current_vad_state == VAD_SPEECH) // 当 VAD 从其他状态切换到有人声时，尝试进入监听。
    {
        // 小智把话说完 我们才能进listenging状态
        if (xiaozhi_handle.current_state == XIAOZHI_STATUS_IDLE) // 要求当前为空闲，避免说话状态下由扬声器回放触发监听。
        {
            xiaozhi_handle.current_state = XIAOZHI_STATUS_LISTENING; // 切换为 LISTENING，使音频发送任务允许上传后续编码包。
            xiaozhi_ws_send_start_listen();                          // 通知服务器开始监听本轮人声，先发送控制消息再持续上传音频。
        }
    }
    else if (xiaozhi_handle.current_vad_state == VAD_SILENCE) // 若 VAD 已切换为静音，则处理一轮人声结束的情况。
    {
        // 这个位置不能无条件回到idle 因为有可能当前这个回调时小智的扬声器里的人声触发的
        if (xiaozhi_handle.current_state != XIAOZHI_STATUS_SPEAKING) // 说话状态下忽略此处结束监听逻辑，避免回放音频干扰会话状态。
        {
            xiaozhi_handle.current_state = XIAOZHI_STATUS_IDLE; // 将业务状态切回空闲，后续编码包不再由发送任务上传。
            xiaozhi_ws_send_stop_listen();                      // 发送 listen/stop，通知服务器当前一轮输入结束。
        }
    }
}

/**
 * @brief 处理主程序注册的 ADC 按键事件。
 * @param button_handle 按键组件传入的设备句柄，本实现未使用。
 * @param user_data 注册时透传的事件编号：1=前键单击，2=后键单击，3=后键长按。
 * 未激活时前键单击重启，以重新查询激活状态；后键长按恢复 Wi-Fi 配置并重启。
 */
void button_cb(void *button_handle, void *user_data)
{
    uint8_t event = (uint8_t)user_data; // 把注册时透传的指针形式编号还原为 uint8_t，用编号区分按键事件。
    if (event == 1)                     // 编号 1 对应前侧 SW2 单击事件。
    {
        // 前面的按键被单击了
        ESP_LOGI("button", "front button single click"); // 输出前侧按键单击日志，方便核对按键响应。
        if (xiaozhi_handle.active_flag == false)         // 只在设备尚未激活时执行下面的重启行为。
        {
            // 在没被激活的状态下 这个按键的功能是重启整个设备
            esp_restart(); // 立即软件重启，使设备重新请求激活状态。
        }
    }

    if (event == 2) // 编号 2 对应后侧 SW3 单击事件。
    {
        // 后面的按键被单击了
        ESP_LOGI("button", "back button single click"); // 当前后键单击只打印日志，没有绑定额外控制功能。
    }

    if (event == 3) // 编号 3 对应后侧 SW3 长按开始事件。
    {
        // 后面的按键被长按了
        ESP_LOGI("button", "back button long press start"); // 打印后侧按键开始长按的日志。
        xiaozhi_wifi_reset();                               // 恢复 Wi-Fi 默认配置并重启，为下一次启动重新配网做准备。
    }
}

/**
 * @brief 在 PSRAM 中创建音频流水线使用的三个常驻环形缓冲区。
 * SR→编码器为 8 KiB BYTEBUF 字节流，可分次取数后拼成 60 ms PCM 帧。
 * 编码器输出和解码器输入各为 16 KiB NOSPLIT，按完整数据项保留 Opus 包边界。
 * 这些是请求容量，NOSPLIT 的实际可用负载还要考虑数据项头部和对齐开销。
 */
void xiaozhi_create_all_ringbuffers(void)
{
    xiaozhi_handle.sr2encoder_ringbuffer_handle = xRingbufferCreateWithCaps(8 * 1024, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM); // 在 PSRAM 创建 8 KiB PCM 字节流缓冲，连接 AFE 输出与 Opus 编码输入。
    xiaozhi_handle.encoder_out_ring_h = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);          // 在 PSRAM 创建 16 KiB NOSPLIT 编码输出缓冲，保留每个 Opus 数据项边界。
    xiaozhi_handle.decoder_in_ring_h = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);           // 在 PSRAM 创建 16 KiB NOSPLIT 解码输入缓冲，接收服务器下行音频包。
}

/**
 * @brief 保留的编码链路调试任务，当前循环只取出并丢弃编码器输出。
 * @param arg 未使用的 FreeRTOS 任务参数。
 * 原本转发到解码器的语句已被注释；app_main 仍创建此任务。
 * 它与 ws_send_audio_task 消费同一缓冲区，两个任务会竞争数据项，并非各得到一份副本。
 */
void encoder_decoder_test_task(void *arg)
{
    // 从编码器的输出缓冲区 读取数据 发给 解码器的输入缓冲区
    ESP_LOGI("main", "啊啊啊"); // 打印当前保留的调试日志，用于观察测试任务是否启动。
    while (1)                   // 循环消费编码输出，当前有效代码不会把数据再送到解码器。
    {
        size_t data_size = 0;                                                                          // 准备接收本次编码数据项的字节长度。
        void *data = xRingbufferReceive(xiaozhi_handle.encoder_out_ring_h, &data_size, portMAX_DELAY); // 从编码输出缓冲取一项，因与发送任务共用队列，两者会竞争音频包。
        // ESP_LOGI("main", "123321");
        // xRingbufferSend(xiaozhi_handle.decoder_in_ring_h, data, data_size, portMAX_DELAY);
        vRingbufferReturnItem(xiaozhi_handle.encoder_out_ring_h, data); // 立即归还数据项，相当于丢弃本次音频，而不是给网络任务再保留副本。
    }
}

/**
 * @brief ESP-IDF 应用入口，按模块依赖顺序初始化设备并启动音频任务。
 * 顺序：缓冲区/业务回调 → 屏幕 → Wi-Fi（等待获取 IP）→ 按键 → 激活查询。
 * 随后初始化 WebSocket、唤醒任务、AFE 与 Opus 编解码，最后启动音频消费者。
 * 任务创建完成后本入口结束，后续业务由 FreeRTOS 任务与组件回调共同驱动。
 */
void app_main(void)
{
    xiaozhi_create_all_ringbuffers(); // 先创建全部音频环形缓冲，确保随后启动的任务能找到有效通道。

    // 0. 创建一个用以管理小智业务的事件标志组
    xiaozhi_handle.event_group_h = xEventGroupCreate();             // 创建全局业务事件组，用不同事件位协调 WebSocket 连接与 hello 应答。
    xiaozhi_handle.wake_callback = wake_cb;                         // 注册唤醒词回调，将 AFE 检测结果转成独立工作任务通知。
    xiaozhi_handle.ws_textframe_process = ws_textframe_process;     // 注册文本帧业务处理函数，负责字幕、表情、状态与 IoT 命令。
    xiaozhi_handle.ws_binaryframe_process = ws_binaryframe_process; // 注册二进制帧业务处理函数，负责将下行音频复制给解码器。
    xiaozhi_handle.vad_changed_callback = vad_changed_cb;           // 注册 VAD 变化回调，负责人声开始、静音结束时的监听控制。
    // 1. 各种初始化
    // 1.1 先初始化lvgl
    xiaozhi_display_init(); // 初始化 LCD、LVGL 及标签，确保配网和激活流程可以显示提示。
    // 1.2 再初始化wifi
    xiaozhi_wifi_init(); // 启动网络与 BLE 配网流程，并阻塞到 STA 获取 IP 后才返回。
    // 1.3 初始化按键
    xiaozhi_button_init();                                                          // 创建两个 ADC 逻辑按键，后续才能注册事件回调。
    xiaozhi_button_front_register_cb(BUTTON_SINGLE_CLICK, button_cb, (void *)1);    // 给前键单击注册 button_cb，传入编号 1 标记该事件。
    xiaozhi_button_back_register_cb(BUTTON_SINGLE_CLICK, button_cb, (void *)2);     // 给后键单击注册同一回调，传入编号 2 区分来源。
    xiaozhi_button_back_register_cb(BUTTON_LONG_PRESS_START, button_cb, (void *)3); // 给后键长按开始注册同一回调，传入编号 3 触发重新配网。
    // 1.4 配网成功就删除二维码
    xiaozhi_display_delete_qrcode(); // 联网后删除屏幕中央的配网二维码，恢复后续业务显示空间。

    // xiaozhi_display_test();
    // 2. 去拿了激活信息
    xiaozhi_active_request(); // 通过 HTTPS 请求激活状态和 WSS 地址、令牌等连接参数。
    // 2.1 xiaozhi的句柄里现在有激活信息
    if (xiaozhi_handle.active_flag == false) // 如果响应表明设备尚未激活，则显示激活码及操作提示。
    {
        char *temp_buffer = (char *)calloc(60, 1);                                               // 申请并清零 60 字节临时文本区，用于格式化激活提示。
        sprintf(temp_buffer, "请到官网控制台激活设备\n 激活码: %s", xiaozhi_handle.active_code); // 把提示文字与激活码拼到临时缓冲；当前使用 sprintf，不进行容量检查。
        xiaozhi_display_text(temp_buffer);                                                       // 将生成的激活提示交给字幕标签，LVGL 会复制字符串。
        free(temp_buffer);                                                                       // 显示接口返回后释放临时文本区，不影响已复制的标签内容。
        xiaozhi_display_tip("激活后单击sw2按键");                                                // 在屏幕顶部提示用户激活后单击 SW2，触发重启并重新查询。
    }

    // 刚开机时为空闲状态
    xiaozhi_handle.current_state = XIAOZHI_STATUS_IDLE;
    xiaozhi_ws_init();
    // ESP-IDF 此处任务栈大小以字节为单位；唤醒任务固定到核心 1，栈放在 PSRAM。
    if (xTaskCreatePinnedToCoreWithCaps(wake_task, "wake_task", 8192, NULL, 5, &wake_task_handle, 1, MALLOC_CAP_SPIRAM) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create wake task");
        return;
    }
    xiaozhi_sr_init();

    xiaozhi_opus_encoder_init();
    xiaozhi_opus_decoder_init();
    // 当前保留并启动了调试消费者，它会与下面的网络发送任务竞争同一编码输出。
    xTaskCreatePinnedToCoreWithCaps(encoder_decoder_test_task, "encoder_decoder_test_task", 32 * 1024, NULL, 16, NULL, 1, MALLOC_CAP_SPIRAM);
    // 创建发送音频的任务
    xTaskCreatePinnedToCoreWithCaps(ws_send_audio_task, "ws_send_audio_task", 4096, NULL, 16, NULL, 1, MALLOC_CAP_SPIRAM);
}
