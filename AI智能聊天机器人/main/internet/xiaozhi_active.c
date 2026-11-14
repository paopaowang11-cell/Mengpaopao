/**
 * @file xiaozhi_active.c
 * 设备激活查询模块：持久化身份、构造 HTTPS 请求并解析连接参数。
 * 响应写入全局业务句柄，为 main 的激活提示和 WebSocket 初始化提供数据。
 */
#include "internet/xiaozhi_active.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include <stdlib.h>
#include "esp_tls.h"
#include "common/common_types.h"

esp_http_client_handle_t client;

#define TAG "xiaozhi_active"

static char *output_buffer = NULL; // HTTP 响应正文缓冲区，由头部事件分配。
static int output_len = 0;         // 响应 Content-Length 声明的总字节数。
// 累计已拷贝的响应字节数，用作下一段正文的写入偏移；当前代码未为重复请求重置它。
static uint32_t currnet_total_len = 0;

/**
 * @brief 接收激活查询的 HTTP 事件，拼接响应正文并触发 JSON 解析。
 * @param evt HTTP 客户端提供的事件及本次收到的头部/正文数据。
 * @return ESP_OK，表示本回调已处理事件。
 * 当前实现依赖 Content-Length 头先分配缓冲区，再按 ON_DATA 的到达顺序追加数据。
 * output_buffer 由本模块分配和释放，按启动阶段的一次请求流程使用。
 */
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{

    switch (evt->event_id) // 按 HTTP 事件编号分派处理，同一请求会多次进入不同事件分支。
    {
    case HTTP_EVENT_ERROR: // 客户端发生错误时进入此分支，当前只记录调试日志。
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR"); // 输出 HTTP 错误事件名称，具体原因在后续断开事件中查询。
        break; // 结束本分支，避免继续执行后面的其他 case。
    case HTTP_EVENT_ON_CONNECTED: // HTTP 底层连接建立后进入此分支。
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED"); // 记录已连接事件，便于观察请求生命周期。
        break; // 结束连接事件处理，不继续落入发送头部事件。
    case HTTP_EVENT_HEADER_SENT: // 请求头发送完成后进入此分支。
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT"); // 记录请求头已经发送，当前不在此处修改请求内容。
        break; // 结束请求头发送事件处理。
    case HTTP_EVENT_ON_HEADER: // 每收到一个响应头字段，都会进入此分支。
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value); // 打印当前响应头的键和值，用于查看服务器声明的元信息。
        if (strcmp(evt->header_key, "Content-Length") == 0) // 仅在头字段名称等于 Content-Length 时读取正文总长度。
        {
            // 获取数据的长度
            output_len = atoi(evt->header_value); // 把十进制长度文本转换成整数，保存为预期响应字节数。
            // 额外分配 1 字节保存字符串结束符，后续日志与 JSON 解析都使用此正文。
            output_buffer = malloc(output_len + 1); // 按正文长度再加 1 字节申请堆空间，额外位置用于字符串结束符。
            output_buffer[output_len] = '\0'; // 预先在正文末尾放置空字符，使接收完成后可作为 C 字符串打印。
        }
        break; // 结束本次响应头处理，继续等待后续头部或正文事件。
    case HTTP_EVENT_ON_DATA: // 收到一段响应正文时进入此分支，一次请求可能分多段到达。
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len); // 打印本次分段长度，区别于整份正文的 Content-Length。
        // HTTP 正文可能分多次 ON_DATA 到达，evt->data_len 只表示本次分段长度。
        memcpy(output_buffer + currnet_total_len, evt->data, evt->data_len); // 把当前分段追加到已接收字节之后；本实现按预分配空间足够的路径使用。
        currnet_total_len += evt->data_len; // 累计本次字节数，使下一段写入位置继续向后推进。

        break; // 结束本次正文分段处理，等待余下数据或完成事件。
    case HTTP_EVENT_ON_FINISH: // 整份响应接收完成后进入此分支。
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH"); // 记录 HTTP 响应完成，接下来可以解析完整 JSON。
        ESP_LOGI(TAG, "res: %s", output_buffer); // 把接收缓冲中的完整响应按字符串输出到日志。
        xiaozhi_active_parse_response(output_buffer, output_len); // 按声明的正文长度解析响应，提取 WSS 参数与激活状态。
        if (output_buffer != NULL) // 释放前检查缓冲区指针是否非空。
        {

            free(output_buffer); // 归还响应正文占用的堆内存，业务长期数据已由解析函数另行复制。
            output_buffer = NULL; // 释放后清空指针，避免断开事件再次释放同一块内存。
        }
        output_len = 0; // 清零记录的正文总长度；累计偏移 currnet_total_len 在此没有同步重置。
        break; // 结束完成事件处理。
    case HTTP_EVENT_DISCONNECTED: // 连接断开时进入此分支，执行错误读取与接收缓冲清理。
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED"); // 输出连接断开日志，说明本次传输连接已结束。
        int mbedtls_err = 0; // 为 mbedTLS 具体错误码准备输出变量，初值为 0。
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL); // 从事件的 TLS 错误句柄读取并清除错误，同时接收 mbedTLS 错误码。
        if (err != 0) // 只有上层错误码非零时才打印详细错误信息。
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err); // 以十六进制打印 ESP-TLS 层错误码。
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err); // 以十六进制打印 mbedTLS 层错误码，辅助排查证书或握手问题。
        }
        if (output_buffer != NULL) // 如果完成事件尚未释放正文缓冲，则在断开路径补充清理。
        {
            free(output_buffer); // 释放仍然存在的响应缓冲，减少异常断开留下的堆占用。
            output_buffer = NULL; // 将缓冲指针清空，避免后续重复使用失效地址。
        }
        output_len = 0; // 重置正文长度，保留当前一次性请求流程的清理行为。
        break; // 结束断开事件处理。
    case HTTP_EVENT_REDIRECT: // 服务器要求重定向时进入此分支。
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT"); // 打印重定向事件名称，便于观察地址跳转。
        esp_http_client_set_header(evt->client, "From", "user@example.com"); // 给重定向请求设置当前保留的 From 示例头字段。
        esp_http_client_set_header(evt->client, "Accept", "text/html"); // 将重定向请求的 Accept 设为 text/html，沿用现有事件处理逻辑。
        esp_http_client_set_redirection(evt->client); // 让 HTTP 客户端按响应中的重定向地址更新目标请求。
        break; // 结束重定向事件处理。
    }
    return ESP_OK; // 向客户端返回 ESP_OK，表示事件回调已经执行完毕。
}

// 不写初始化了 因为http客户端是一次性的
/**
 * @brief 向激活/OTA 查询接口发送同步 HTTPS POST，获取设备业务连接信息。
 * 先设置设备身份请求头和 JSON 请求体，perform 期间由事件回调接收并解析响应。
 * 此函数查询激活状态与连接参数，没有执行固件下载或 OTA 分区切换。
 * 应在 Wi-Fi 获取 IP 后、WebSocket 初始化前调用。
 */
void xiaozhi_active_request(void)
{
    // 0. 初始化
    esp_http_client_config_t config = { // 构建 HTTPS 客户端配置，描述激活查询的目标、方法和回调。
        .host = "api.tenclass.net", // 目标服务器主机名为 api.tenclass.net，不在 host 中拼接路径。
        .path = "/xiaozhi/ota/", // 目标路径为 /xiaozhi/ota/，本项目通过此接口查询激活与连接信息。
        .transport_type = HTTP_TRANSPORT_OVER_SSL, // 采用 SSL 传输，即 HTTPS，而非明文 HTTP。
        .event_handler = _http_event_handler, // 注册响应事件处理函数，正文拼接与解析在该回调中完成。
        .crt_bundle_attach = esp_crt_bundle_attach, // 附加 ESP 证书包，为 TLS 服务端证书验证提供信任根。
        .method = HTTP_METHOD_POST, // 使用 POST 方法，提交 JSON 设备信息。
    };
    client = esp_http_client_init(&config); // 根据配置创建 HTTP 客户端，保存句柄供请求头和正文设置函数使用。

    // 设置请求头
    xiaozhi_active_set_request_header(); // 添加内容类型、MAC、UUID 等请求头，告诉服务器设备身份。
    // 设置请求体
    xiaozhi_active_set_request_body(); // 构造并设置应用版本、摘要及板卡信息组成的 JSON 请求体。
    // 发送请求
    esp_err_t err = esp_http_client_perform(client); // 同步执行完整请求，期间触发事件回调；返回值存入 err，当前未继续判断。
}

uint8_t mac[6];

// MAC 文本 17 字符加结束符；UUID 文本 36 字符加结束符，均为全局常驻数组。
char mac_str[18];
char uuid_str[37];
/**
 * @brief 为已创建的 HTTP 客户端设置 JSON 类型、固件标识和设备身份。
 * Device-Id 为 STA MAC，Client-Id 为从 NVS 读取或首次生成的 UUID。
 * 全局句柄引用静态字符数组，供后续 WebSocket 握手重复使用。
 */
void xiaozhi_active_set_request_header(void)
{
    esp_http_client_set_header(client, "Content-Type", "application/json"); // 声明请求正文是 JSON，便于服务器按对应格式解析。
    esp_http_client_set_header(client, "Host", "api.tenclass.net"); // 设置 HTTP Host 为当前服务域名。
    esp_http_client_set_header(client, "User-Agent", "bread-compact-wifi-128x64/1.0.1"); // 设置当前板型及固件版本对应的 User-Agent 标识。

    esp_wifi_get_mac(WIFI_IF_STA, mac); // 读取 STA 接口的 6 字节 MAC 地址，作为设备身份基础。
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); // 将 6 个字节格式化为冒号分隔的 17 字符小写十六进制文本。
    ESP_LOGW("xiaozhi_active", "Device-Id: %s", mac_str); // 打印 Device-Id，方便在控制台或服务端核对设备。
    xiaozhi_handle.mac_str = mac_str; // 让全局业务句柄引用常驻 mac_str 数组，后续 WSS 连接复用它。
    esp_http_client_set_header(client, "Device-Id", mac_str); // 把 MAC 文本设置为激活请求的 Device-Id 头字段。
    xiaozhi_active_uuid_v4_gen(uuid_str); // 从 NVS 恢复 UUID；尚无保存值时生成并持久化新的 UUID。
    xiaozhi_handle.uuid = uuid_str; // 让全局业务句柄引用常驻 UUID 字符数组。
    esp_http_client_set_header(client, "Client-Id", uuid_str); // 把 UUID 设置为 Client-Id，与后续 WebSocket 身份保持一致。
}
// 7b94d69a - 9808 - 4c59 - 9c9b - 704333b38aff
/**
 * @brief 优先读取 NVS 中保存的 UUID，不存在时生成并写入持久化存储。
 * @param uuid_str 输出缓冲区，至少 37 字节，容纳 36 个字符和结尾空字符。
 * 使用 xiaozhi_data 命名空间的 uuid 键；随机数格式化时设置 UUID v4 版本和变体位。
 */
void xiaozhi_active_uuid_v4_gen(char *uuid_str)
{
    // 0. 开启nvs的一个命名空间
    nvs_handle_t nvs_handle; // 声明 NVS 命名空间句柄，稍后由 nvs_open 输出有效值。
    esp_err_t err = nvs_open("xiaozhi_data", NVS_READWRITE, &nvs_handle); // 以可读写方式打开 xiaozhi_data 命名空间，必要时用于保存新 UUID。
    if (err != ESP_OK) // 检查打开是否失败，失败时不能继续访问无效 NVS 句柄。
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err)); // 把错误码转换成名称并打印，便于定位存储初始化问题。
        return; // 结束生成流程，调用方输出缓冲不会在此路径中得到新 UUID。
    }
    // 1. 先去nvs中 看一下有没有之前就创建好的uuid
    size_t required_size; // 声明字符串所需容量变量，nvs_get_str 会写入包含结束符的大小。
    err = nvs_get_str(nvs_handle, "uuid", NULL, &required_size); // 用 NULL 数据地址先查询 uuid 键是否存在及其所需缓冲大小。

    if (err == ESP_OK) // 查询成功表示已有 UUID，直接走恢复路径保持设备身份稳定。
    {
        // 2. 如果有 就从nvs中读取上一次生成的结果 继续使用
        ESP_LOGI(TAG, "当前用的是上一次的uuid"); // 记录当前沿用上一次保存的 UUID。
        nvs_get_str(nvs_handle, "uuid", uuid_str, &required_size); // 按查询得到的容量把 UUID 字符串读入调用方缓冲区。
    }
    else // 查询未成功时进入生成新 UUID 的路径，当前未进一步区分查询错误原因。
    {
        // 3. 如果没有 就生成一个新的 uuid
        ESP_LOGI(TAG, "当前用的是新生成的uuid"); // 记录当前即将创建一个新的 UUID。
        // 生成uuid v4 16个字节
        uint32_t rand1 = esp_random(); // 取得第一组 32 位随机数，用于 UUID 前段。
        uint32_t rand2 = esp_random(); // 取得第二组随机数，用于后续分组及版本字段。
        uint32_t rand3 = esp_random(); // 取得第三组随机数，用于变体字段和后续分组。
        uint32_t rand4 = esp_random(); // 取得第四组随机数，用于 UUID 的末尾 8 个十六进制字符。

        sprintf(uuid_str, // 开始格式化 UUID，将结果写入调用方至少 37 字节的缓冲区。
                "%08x-%04x-%04x-%04x-%04x%08x", // 按 8-4-4-4-12 个十六进制字符分组，中间使用连字符分隔。
                (unsigned int)(rand1), // 第一组使用 rand1 的 32 位值，转换为与 %x 匹配的 unsigned int。
                (unsigned int)(rand2 >> 16), // 第二组取 rand2 的高 16 位，输出 4 个十六进制字符。
                (unsigned int)(((rand2 >> 12) & 0x0fff) | 0x4000), // 保留 12 个随机位，再置入 0x4000，使本组最高半字节为版本号 4。
                (unsigned int)(((rand3 >> 16) & 0x3fff) | 0x8000), // 保留 14 个随机位，再置入 0x8000，设置 UUID 变体对应的高位模式。
                (unsigned int)(rand3 & 0xffff), // 取 rand3 的低 16 位，作为最后 12 字符分组的前 4 字符。
                (unsigned int)(rand4)); // 用 rand4 组成最后 8 字符，完成 UUID 字符串格式化。
        // 把这个uuid存起来
        nvs_set_str(nvs_handle, "uuid", uuid_str); // 把新 UUID 写到当前命名空间的 uuid 键，准备持久化。
        nvs_commit(nvs_handle); // 提交 NVS 修改，使新 UUID 在后续重启后仍能恢复。
        nvs_close(nvs_handle); // 关闭当前 NVS 句柄；当前代码只在生成分支执行此关闭语句。
    }
}

/**
 * @brief 构建 application 和 board 两部分 JSON，设置为 HTTP POST 正文。
 * application 包含版本及 ELF 摘要；board 的 SSID/RSSI/信道/IP 当前为固定示例值。
 * set_post_field 保存正文指针，请求执行前不能释放打印得到的 res 字符串。
 * cJSON_Delete 释放节点树，不会释放由 cJSON_PrintUnformatted 单独分配的 res。
 */
void xiaozhi_active_set_request_body(void)
{
    // 最外层的json
    cJSON *root = cJSON_CreateObject(); // 创建最外层 JSON 对象，最终请求体由它序列化生成。
    cJSON *application = cJSON_CreateObject(); // 创建 application 子对象，用于固件版本及 ELF 摘要。
    cJSON *board = cJSON_CreateObject(); // 创建 board 子对象，用于板型、身份和网络信息。

    // 子对象挂入 root 后归根节点管理，删除根节点会递归删除这些子节点。
    cJSON_AddItemToObject(root, "application", application); // 把 application 挂到 root 后，其节点生命周期由根对象管理。
    cJSON_AddItemToObject(root, "board", board); // 把 board 挂到 root 后，删除 root 会一并删除这个子对象。

    cJSON_AddStringToObject(application, "version", "1.0.1"); // 填写当前上报的应用版本字符串 1.0.1。
    cJSON_AddStringToObject(application, "elf_sha256", esp_app_get_elf_sha256_str()); // 读取当前固件 ELF 的 SHA-256 文本摘要，写入应用信息。

    cJSON_AddStringToObject(board, "type", "bread-compact-wifi"); // 填写服务端使用的板型标识，当前为 bread-compact-wifi。
    cJSON_AddStringToObject(board, "name", "bread-compact-wifi-128x64"); // 填写更具体的板卡名称，沿用当前固件的协议标识。
    cJSON_AddStringToObject(board, "ssid", "万能wifi钥匙"); // 写入固定示例 SSID，并非通过 Wi-Fi 驱动读取的实时网络名称。
    cJSON_AddNumberToObject(board, "rssi", -55); // 写入固定示例信号强度 -55，单位按协议中的 RSSI 含义使用。
    cJSON_AddNumberToObject(board, "channel", 1); // 写入固定示例信道 1，并非当前连接信道的实时查询结果。
    cJSON_AddStringToObject(board, "ip", "192.168.1.11"); // 写入固定示例 IP 地址字符串，不读取本机实际分配地址。
    cJSON_AddStringToObject(board, "mac", mac_str); // 写入此前读取并格式化的真实 STA MAC 字符串。

    char *res = cJSON_PrintUnformatted(root); // 把整棵 JSON 树序列化为紧凑字符串，res 是另外申请的堆内存。

    esp_http_client_set_post_field(client, res, strlen(res)); // 设置 POST 正文地址和字节长度；客户端保存指针，执行请求前不能释放 res。

    cJSON_Delete(root); // 释放 JSON 节点树，不会同时释放前面单独生成的 res 字符串。

    ESP_LOGI(TAG, "body :%s", res); // 打印已生成的请求正文，便于核对实际提交内容。
}

/**
 * @brief 解析激活查询响应，把 WebSocket URL、令牌及激活状态保存到业务句柄。
 * @param res 响应 JSON 的起始地址。
 * @param len JSON 的有效字节数。
 * URL、token 和激活码复制到独立堆内存，避免后续使用 HTTP 接收缓冲区的悬空指针。
 * 按当前协议约定：含 activation 表示仍需激活；无此字段表示已激活。
 * 激活码缓冲区固定为 7 字节，当前实现按 6 位激活码加结尾空字符处理。
 */
void xiaozhi_active_parse_response(char *res, int len)
{
    cJSON *root = cJSON_ParseWithLength(res, len); // 按照明确的 len 字节解析响应 JSON，生成节点树。
    if (root == NULL) // 检查 JSON 是否解析成功，失败时不能读取字段。
    {
        ESP_LOGE(TAG, "woc 不是json"); // 打印响应不符合 JSON 格式的错误。
        return; // 直接结束解析，此时没有有效根对象需要继续使用。
    }

    cJSON *websocket = cJSON_GetObjectItem(root, "websocket"); // 读取根对象内的 websocket 配置对象。
    if (websocket == NULL) // 检查服务器是否提供了 websocket 字段。
    {
        ESP_LOGE(TAG, "websocket is null"); // 记录缺少 websocket 信息，表明无法取得后续连接参数。
        return; // 结束当前解析；此提前返回路径没有删除已创建的 root。
    }
    cJSON *ws_url = cJSON_GetObjectItem(websocket, "url"); // 从 websocket 对象读取 url 节点。
    if (ws_url == NULL) // 检查是否缺少 WebSocket 地址字段。
    {
        ESP_LOGE(TAG, "ws_url is null"); // 打印缺少 url 的错误，避免静默使用无效地址。
        return; // 提前结束，不继续创建 WebSocket URL 副本。
    }
    // ws_url_str这个东西是从cjson节点里直接返回的
    // 如果json将来被删除 会被跟着释放
    // 所以为了内存安全 必须拷贝。
    char *ws_url_str = cJSON_GetStringValue(ws_url); // 从 URL 节点借用字符串地址，它的存储归该 JSON 树管理。
    xiaozhi_handle.websocket_url = malloc(strlen(ws_url_str) + 1); // 申请 URL 长度加 1 字节的独立空间，包含末尾结束符。
    strcpy(xiaozhi_handle.websocket_url, ws_url_str); // 复制完整 URL 到全局业务句柄，使其不依赖响应接收缓冲区。

    cJSON *token = cJSON_GetObjectItem(websocket, "token"); // 从 websocket 对象读取认证 token 节点。
    if (token == NULL) // 检查 token 是否缺失，后续 WSS 握手需要此字段。
    {
        ESP_LOGE(TAG, "token is null"); // 打印缺少认证令牌的错误。
        return; // 结束当前解析，不继续复制 token 或判断激活信息。
    }
    char *token_str = cJSON_GetStringValue(token); // 从 token 节点借用字符串地址，为下一步复制做准备。
    xiaozhi_handle.websocket_token = malloc(strlen(token_str) + 1); // 按 token 长度加结束符申请独立堆内存。
    strcpy(xiaozhi_handle.websocket_token, token_str); // 将 token 复制到业务句柄，供 Authorization 请求头使用。

    // 解析是否激活

    // 这里依据 activation 字段是否存在判断业务激活状态，不以 HTTP 是否连接成功作为依据。
    cJSON *activation = cJSON_GetObjectItem(root, "activation"); // 读取可选的 activation 对象，以当前协议判断设备是否仍需激活。
    if (activation == NULL) // 没有 activation 对象时，走已激活路径。
    {
        // 如果当前这个json已经又了websocket信息 但是没有激活信息
        //  说明已经激活了
        xiaozhi_handle.active_flag = true; // 把业务激活标志设为 true，表示无需再显示激活码。
    }
    else // 存在 activation 时，走需要用户在控制台激活的路径。
    {
        // 有ws信息，且有激活信息
        // 说明没有激活
        xiaozhi_handle.active_flag = false; // 把业务激活标志设为 false，供主程序选择激活提示界面。
        // 解析出激活码
        xiaozhi_handle.active_code = malloc(7); // 申请 7 字节空间，当前约定保存 6 位激活码及结束符。
        xiaozhi_handle.active_code[6] = '\0'; // 将第 7 个字节预置为空字符，保证预期 6 位码可以结束。
        cJSON *active_code = cJSON_GetObjectItem(activation, "code"); // 从 activation 对象读取 code 节点。
        strcpy(xiaozhi_handle.active_code, active_code->valuestring); // 复制激活码；当前使用 strcpy，按服务端返回码符合既定长度的路径处理。
        ESP_LOGI(TAG, "xiaozhi_handle.ws_url = %s\nxiaozhi_handle.ws_token = %s\nxiaozhi_handle.active_flag = %d\nxiaozhi_handle.active_code = %s\n", // 开始打印已解析的连接与激活信息，下列参数对应格式字符串中的占位符。
                 xiaozhi_handle.websocket_url, // 第一个 %s 使用复制后的 WebSocket URL。
                 xiaozhi_handle.websocket_token, // 第二个 %s 使用复制后的访问令牌。
                 xiaozhi_handle.active_flag, // %d 使用当前激活布尔标志，打印为 0 或 1。
                 xiaozhi_handle.active_code); // 最后一个 %s 使用当前保存的激活码，结束此条日志调用。
    }
}