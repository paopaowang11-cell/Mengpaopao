/**
 * @file xiaozhi_wifi.c
 * Wi-Fi 启动与 BLE 配网模块。
 * 使用 NVS 保留网络凭据，首次启动由手机配网，后续启动直接连接已保存的网络。
 */
#include "internet/xiaozhi_wifi.h"
#include "display/xiaozhi_display.h"

#define TAG "xiaozhi_wifi"

// 模块私有的联网完成事件组，与全局业务句柄中的 WebSocket 事件组不同。
static EventGroupHandle_t s_wifi_event_group;

// 最低位用于通知启动流程已经获取 IP；本实现断线时未清除此位。
#define WIFI_CONNECTED_BIT 1

#define PROV_QR_VERSION "v1"
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

/**
 * @brief 初始化 NVS，为保存 Wi-Fi 凭据及设备 UUID 提供持久化存储。
 * 无可用页或存储版本不兼容时擦除 NVS 分区后重试；该路径会清除其中已有数据。
 */
void xiaozhi_wifi_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init(); // 初始化 NVS Flash，返回值用于判断是否存在存储格式或空间问题。
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) // 仅对无可用页或版本不兼容这两类错误执行擦除重建。
    {
        ESP_ERROR_CHECK(nvs_flash_erase()); // 擦除 NVS 分区并检查结果，此路径会清除其中已有网络凭据及 UUID。
        ret = nvs_flash_init(); // 擦除后重新初始化 NVS，并用新结果覆盖 ret。
    }
    ESP_ERROR_CHECK(ret); // 检查最终初始化结果，非 ESP_OK 时终止正常执行流程。
}

/**
 * @brief 用 STA MAC 后三个字节生成 PROV_XXXXXX 形式的 BLE 配网设备名。
 * @param service_name 调用方提供的输出字符串缓冲区。
 * @param max 输出容量，包含结尾空字符；当前调用提供 12 字节。
 */
static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6]; // 准备 6 字节数组，用于接收 STA 接口的 MAC 地址。
    const char *ssid_prefix = "PROV_"; // 定义配网服务名固定前缀 PROV_，便于手机识别待配网设备。
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac); // 读取 STA MAC 到本地数组，即使此时尚未连接热点也用于设备命名。
    snprintf(service_name, max, "%s%02X%02X%02X", // 按 max 容量格式化名称，每个 %02X 表示两位大写十六进制字节。
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]); // 将前缀和 MAC 后三个字节拼成 PROV_XXXXXX，降低不同设备重名概率。
}

/**
 * @brief 组织 Espressif 配网应用识别的 JSON，并显示屏幕二维码。
 * @param name BLE 配网服务名称。
 * @param username 为统一接口保留的用户名参数，当前 security1 载荷未使用。
 * @param pop 所有权证明口令；非空时写入二维码，供手机建立安全会话。
 * @param transport 配网传输方式，本项目为 ble。
 */
static void wifi_prov_show_qr(const char *name, const char *username, const char *pop, const char *transport)
{
    if (!name || !transport) // 检查服务名和传输类型是否为空，这两个字段是生成配网载荷的必要信息。
    {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing."); // 记录缺少必要数据的警告，避免生成无法识别的二维码。
        return; // 停止二维码生成流程，不访问空字符串指针。
    }
    char payload[150] = {0}; // 创建并清零 150 字节局部缓冲，保存将被编码的配网 JSON 文本。
    if (pop) // 如果提供了 PoP 所有权证明口令，则在二维码载荷中包含它。
    {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" // 开始受容量限制的字符串格式化，写入版本 ver 和服务名 name 字段。
                                           ",\"pop\":\"%s\",\"transport\":\"%s\"}", // 相邻字符串字面量在编译时拼接，继续追加 pop 和 transport 字段并闭合 JSON。
                 PROV_QR_VERSION, name, pop, transport); // 依次用协议版本、设备名、PoP 和传输类型替换四个 %s。
    }
    else // 没有 PoP 时使用不包含 pop 字段的另一种载荷格式。
    {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" // 同样使用 snprintf 限制最大写入容量，先生成版本及服务名字段。
                                           ",\"transport\":\"%s\"}", // 续接 transport 字段并结束 JSON，不写入 pop。
                 PROV_QR_VERSION, name, transport); // 用版本、服务名、传输类型依次填充该格式的三个占位符。
    }
    ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning."); // 打印提示，告知可使用配网手机应用扫描二维码。
    // esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    // esp_qrcode_generate(&cfg, payload);
    xiaozhi_display_tip("请扫描二维码配网"); // 更新屏幕顶部提示，告诉用户需要扫码配网。
    xiaozhi_display_show_qrcode(payload, strlen(payload)); // 将 JSON 文本及不含结束符的长度交给显示模块，生成屏幕二维码。
    ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL, payload); // 输出备用二维码网页地址及载荷，便于通过浏览器查看对应内容。
}

/**
 * @brief 在 ESP 默认事件循环中处理 Wi-Fi、IP、配网、BLE 和安全会话事件。
 * @param arg 注册事件时的用户参数，当前为 NULL。
 * @param event_base 事件所属类别，先按类别分流再判断 event_id。
 * @param event_id 类别内的具体事件编号。
 * @param event_data 由组件提供的事件专用结构，只在对应事件中按正确类型读取。
 * 启动/断线时发起连接，获取 IP 后置位初始化等待标志，配网结束后释放管理器。
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) // 先判断事件属于 Wi-Fi 类别，避免把不同事件源的相同编号混在一起。
    {
        if (event_id == WIFI_EVENT_STA_START) // STA 驱动启动后处理这个事件，开始首次热点连接。
        {
            // 如果能够成功进入Station模式
            ESP_LOGI(TAG, "wifi已经进入station模式 正在尝试发起连接"); // 打印已进入 Station 模式并准备连接的日志。
            esp_wifi_connect(); // 按已保存或配网管理器设置的配置发起 Wi-Fi 连接。
        }

        if (event_id == WIFI_EVENT_STA_DISCONNECTED) // 检测到 STA 与热点断开时进入此重连分支。
        {
            ESP_LOGW(TAG, "wifi断了 再次尝试发起连接..."); // 记录网络断开，便于观察是否频繁重连。
            esp_wifi_connect(); // 再次发起连接，当前实现没有在此处设置退避或次数限制。
        }
    }

    if (event_base == IP_EVENT) // 判断是否为 IP 地址相关事件，与底层 Wi-Fi 连接事件区分。
    {
        if (event_id == IP_EVENT_STA_GOT_IP) // 只处理 STA 获得 IPv4 地址的通知，表示网络接口具备后续通信条件。
        {
            ESP_LOGI(TAG, "哇，连上了！"); // 打印已获取网络连接的业务日志。
            // 将事件数据转为获取 IP 事件结构，用 IPSTR/IP2STR 宏格式化 IPv4 地址。
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data; // 按获取 IP 事件结构解释 event_data，以便读取其中的地址信息。
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip)); // IPSTR 提供格式、IP2STR 展开四段地址，打印点分十进制 IPv4。
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT); // 置位联网完成事件，解除 xiaozhi_wifi_init 中的启动等待。
        }
    }

    if (event_base == WIFI_PROV_EVENT) // 判断是否属于 Wi-Fi 配网管理器的事件。
    {
        switch (event_id) // 根据配网事件编号分别处理开始、收凭据、失败、成功和结束。
        {
        case WIFI_PROV_START: // 配网流程开始时进入该分支。
            ESP_LOGI(TAG, "Provisioning started"); // 记录配网服务已经启动，等待手机提交热点信息。
            break; // 结束配网开始事件分支，不继续落入后面的 case。
        case WIFI_PROV_CRED_RECV: // 收到手机传来的 Wi-Fi 凭据时进入该分支。
        { // 配网收到了wifi名和密码 但是 啥也不干 只打日志
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data; // 将事件数据解释为 STA 配置，取得 SSID 和密码数组。
            ESP_LOGI(TAG, "Received Wi-Fi credentials" // 开始打印收到凭据的日志，后续相邻字面量构成同一格式字符串。
                          "\n\tSSID     : %s\n\tPassword : %s", // 继续指定换行、缩进和两个字符串占位符，用于显示 SSID 及密码。
                     (const char *)wifi_sta_cfg->ssid, // 将 SSID 字节数组转换为字符指针，填入第一个 %s。
                     (const char *)wifi_sta_cfg->password); // 将密码字节数组转换为字符指针，填入第二个 %s 并结束日志调用。
            break; // 结束凭据接收事件处理，连接和验证工作由管理器及驱动继续执行。
        }
        case WIFI_PROV_CRED_FAIL: // 手机提交的凭据验证失败时进入该分支。
        { // 如果失败才会走这个逻辑 打印一下失败的原因 然后把配网管理器的状态重置
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data; // 把事件数据解释为失败原因枚举指针。
            ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s" // 开始输出配网失败日志，并为具体原因预留 %s。
                          "\n\tPlease reset to factory and retry provisioning", // 续接现有重试提示文字，与上一行字符串在编译时合并。
                     (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed" : "Wi-Fi access-point not found"); // 若为认证失败就打印密码/认证错误，否则打印找不到接入点的说明。
            // 如果配网失败了 就把配网管理器复位一下
            wifi_prov_mgr_reset_sm_state_on_failure(); // 复位配网管理器失败状态机，使流程能够继续接收后续尝试。
            break; // 结束凭据失败分支。
        }
        case WIFI_PROV_CRED_SUCCESS: // Wi-Fi 凭据被验证成功时进入此分支。
            ESP_LOGI(TAG, "Provisioning successful"); // 记录配网凭据验证成功；是否获得 IP 仍由 IP_EVENT 单独通知。
            break; // 结束凭据成功事件处理。
        case WIFI_PROV_END: // 配网服务结束时进入此资源清理分支。
            /* 配网完成后反初始化管理器，释放配网服务资源。 */
            wifi_prov_mgr_deinit(); // 反初始化配网管理器，释放配网服务及方案回调管理的资源。
            break; // 结束配网结束事件处理。
        default: // 未明确列出的配网事件走默认分支。
            break; // 忽略其余配网事件，不改变当前业务状态。
        }
    }

    // BLE 传输连接状态用于配网诊断，与 STA 是否已经获得 IP 不是同一个状态。
    if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) // 判断是否为手机与设备之间 BLE 配网传输的连接事件。
    {
        switch (event_id) // 按 BLE 事件编号区分已连接与已断开。
        {
        case PROTOCOMM_TRANSPORT_BLE_CONNECTED: // BLE 配网客户端连接上设备时进入此分支。
            ESP_LOGI(TAG, "BLE transport: Connected!"); // 打印 BLE 传输连接建立日志，这不表示 STA 已连接路由器。
            break; // 结束 BLE 已连接事件处理。
        case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED: // BLE 配网客户端断开时进入此分支。
            ESP_LOGI(TAG, "BLE transport: Disconnected!"); // 打印 BLE 传输断开日志，不直接更改 Wi-Fi 业务状态。
            break; // 结束 BLE 已断开事件处理。
        default: // 未列出的 BLE 传输事件走默认分支。
            break; // 忽略其他 BLE 事件。
        }
    }

    // 安全会话事件描述手机与设备的配网认证结果，独立于 Wi-Fi 密码认证结果。
    if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) // 判断是否属于手机与设备配网安全会话的认证事件。
    {
        switch (event_id) // 根据安全会话事件编号区分成功、参数错误及凭据不匹配。
        {
        case PROTOCOMM_SECURITY_SESSION_SETUP_OK: // 安全会话成功建立时进入该分支。
            ESP_LOGI(TAG, "Secured session established!"); // 记录配网传输的安全会话已建立，手机可继续发送受保护的配置数据。
            break; // 结束安全会话成功事件处理。
        case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS: // 收到无效安全参数时进入该错误分支。
            ESP_LOGE(TAG, "Received invalid security parameters for establishing secure session!"); // 记录安全参数无效，便于检查手机端与设备端配置是否匹配。
            break; // 结束安全参数错误事件处理。
        case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH: // 用户名或 PoP 等配网会话凭据不匹配时进入该分支。
            ESP_LOGE(TAG, "Received incorrect username and/or PoP for establishing secure session!"); // 记录安全会话认证信息不匹配，与连接路由器的 Wi-Fi 密码错误区分。
            break; // 结束凭据不匹配事件处理。
        default: // 其余安全会话事件进入默认分支。
            break; // 忽略未列出的安全会话事件。
        }
    }
}

/**
 * @brief 初始化网络栈、事件循环与 BLE 配网服务，并等待 STA 获取 IP。
 * 未配网时显示二维码等待手机传入凭据；已配网时直接使用保存的配置连接。
 * 这是阻塞式启动流程，xEventGroupWaitBits 使用 portMAX_DELAY，没有初始化超时。
 * 返回只说明启动阶段曾获得 IP，后续断线由事件回调自动尝试重连。
 */
void xiaozhi_wifi_init(void)
{
    xiaozhi_wifi_nvs_init(); // 先初始化 NVS，确保网络凭据检查及后续 UUID 持久化可用。
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA"); // 打印当前采用 STA 模式的启动日志。
    s_wifi_event_group = xEventGroupCreate(); // 创建本模块事件组，稍后用于阻塞等待 STA 获取 IP。

    // 初始化 ESP-NETIF 网络接口抽象及底层 TCP/IP 栈。
    ESP_ERROR_CHECK(esp_netif_init()); // 初始化 ESP-NETIF 及相关网络栈资源，并检查结果。
    // 创建默认的事件循环任务
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // 创建默认事件循环，Wi-Fi、IP 和配网通知在这里分派。

    // 创建默认 STA 网络接口，然后初始化 Wi-Fi 驱动。
    esp_netif_create_default_wifi_sta(); // 创建默认 Wi-Fi STA 网络接口，并把它接到网络协议栈。
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // 使用组件宏获取 Wi-Fi 驱动默认初始化配置。
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // 按默认配置初始化 Wi-Fi 驱动，并检查返回码。

    // 给事件循环注册回调函数
    esp_event_handler_instance_t instance_any_id; // 声明 Wi-Fi 全事件注册实例句柄，供注册 API 输出。
    esp_event_handler_instance_t instance_got_ip; // 声明获取 IP 事件的注册实例句柄。
    esp_event_handler_instance_t instance_security_session; // 声明安全会话事件的注册实例句柄。
    esp_event_handler_instance_t instance_ble_transport; // 声明 BLE 传输事件的注册实例句柄。
    esp_event_handler_instance_t instance_wifi_prov; // 声明配网管理器事件的注册实例句柄。

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, // 开始注册 WIFI_EVENT 回调，并由 ESP_ERROR_CHECK 检查完整调用结果。
                                                        ESP_EVENT_ANY_ID, // ESP_EVENT_ANY_ID 表示接收该事件源下的所有 Wi-Fi 事件。
                                                        &event_handler, // 统一使用 event_handler 处理这一类事件。
                                                        NULL, // 不附带额外用户参数，回调的 arg 将为 NULL。
                                                        &instance_any_id)); // 将此次注册实例写到 instance_any_id，完成 Wi-Fi 回调注册。
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, // 开始注册 IP_EVENT 事件源的回调。
                                                        IP_EVENT_STA_GOT_IP, // 只订阅 STA 获得 IP 地址的事件，不接收该类别的其他事件。
                                                        &event_handler, // 仍交给统一 event_handler，函数内部会先判断 event_base。
                                                        NULL, // 该 IP 回调同样不传入额外用户上下文。
                                                        &instance_got_ip)); // 保存 IP 回调注册实例，完成本次注册调用。

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_wifi_prov)); // 订阅所有配网管理器事件，统一处理开始、凭据验证及结束等通知。

    ESP_ERROR_CHECK(esp_event_handler_instance_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_ble_transport)); // 订阅所有 BLE 配网传输事件，用于记录手机连接状态。

    ESP_ERROR_CHECK(esp_event_handler_instance_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_security_session)); // 订阅所有配网安全会话事件，用于记录认证建立或失败原因。

    // 配网管理器的配置

    wifi_prov_mgr_config_t config = { // 定义配网管理器的连接重试和 BLE 方案配置。
        .wifi_prov_conn_cfg = { // 开始设置 Wi-Fi 凭据验证过程中的连接策略。
            .wifi_conn_attempts = 5, // 把该配置中的热点连接尝试次数设为 5。
        },

        .scheme = wifi_prov_scheme_ble, // 选择 BLE 配网方案，手机通过蓝牙向设备发送 Wi-Fi 信息。

        // 配网管理器结束后按 BLE 方案释放蓝牙控制器资源，后续只保留 Wi-Fi 业务。
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM}; // 使用结束后释放 BTDM 资源的方案事件处理器，减少配网后蓝牙内存占用。
    // 初始化配网管理器
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config)); // 初始化配网管理器，传入完整配置并检查结果。

    // 布尔值 用来判断有没有配过网
    bool provisioned = false; // 先假设没有配过网，随后由管理器查询结果覆盖。

    // 执行是否配过网的检查
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned)); // 检查 NVS/驱动中是否已有配网配置，并写入 provisioned。

    if (!provisioned) // 未保存配网配置时，启动手机 BLE 配网流程。
    {
        // 如果没有配过网
        // 1. 打印日志 说开始配网
        ESP_LOGI(TAG, "Starting provisioning"); // 打印开始配网的日志。
        // 2. 定义一个设备名称 蓝牙搜索的时候会显示这个设备名
        char service_name[12]; // 准备 12 字节服务名缓冲，可容纳 PROV_ 加 6 位十六进制及结束符。
        get_device_service_name(service_name, sizeof(service_name)); // 基于 STA MAC 后三字节生成本机的 BLE 配网名称。

        // 3. 启动安全会话版本1
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1; // 选择配网安全协议版本 1，让手机端采用相同安全会话方式。
        // 4. 定义 PoP（所有权证明）口令，手机需使用相同值建立 security1 安全会话。
        const char *pop = "abcd1234"; // 设置 PoP 所有权证明口令，二维码与设备认证使用同一值。
        // 5. 把pop放到安全配置中
        wifi_prov_security1_params_t *sec_params = pop; // 将 PoP 按 security1 参数类型传给管理器，保持当前组件接口约定。
        //
        const char *username = NULL; // security1 当前不用用户名，因此保留为 NULL。
        // 7. 给蓝牙的数据字段定义一个uuid，这个uuid不能随便改的
        //    因为esp的配网软件会直接从这个uuid下查数据
        uint8_t custom_service_uuid[] = { // 定义 128 位 BLE 服务 UUID 数组，按低有效字节在前排列。
            /* BLE 服务 UUID 以低有效字节在前（LSB→MSB）的顺序填写。 */
            0xb4, // UUID 的第 1 字节（最低有效字节）为 0xB4。
            0xdf, // UUID 的第 2 字节为 0xDF，与其余字节共同标识配网服务。
            0x5a, // UUID 的第 3 字节为 0x5A，保持约定的完整服务标识。
            0x1c, // UUID 的第 4 字节为 0x1C。
            0x3f, // UUID 的第 5 字节为 0x3F。
            0x6b, // UUID 的第 6 字节为 0x6B。
            0xf4, // UUID 的第 7 字节为 0xF4。
            0xbf, // UUID 的第 8 字节为 0xBF。
            0xea, // UUID 的第 9 字节为 0xEA。
            0x4a, // UUID 的第 10 字节为 0x4A。
            0x82, // UUID 的第 11 字节为 0x82。
            0x03, // UUID 的第 12 字节为 0x03。
            0x04, // UUID 的第 13 字节为 0x04。
            0x90, // UUID 的第 14 字节为 0x90。
            0x1a, // UUID 的第 15 字节为 0x1A。
            0x02, // UUID 的第 16 字节（最高有效字节）为 0x02，至此组成完整 128 位值。
        };
        // 8. 把刚才的uuid直接设置进去
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid); // 将上述 UUID 设置为本次 BLE 配网服务的标识，供手机端识别服务。
        // 9. 开启配网服务
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, NULL)); // 以安全协议、PoP 和服务名启动配网；最后 NULL 不指定 SoftAP 服务密钥。
        wifi_prov_show_qr(service_name, username, pop, "ble"); // 生成包含名称、PoP 和 ble 传输方式的二维码，交给屏幕显示。
    }
    else // 如果此前已经配网，则跳过手机配网服务，直接启动 STA。
    {
        // 如果配过网
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA"); // 记录已有配置，准备连接之前保存的热点。
        // 反初始化配网管理器
        wifi_prov_mgr_deinit(); // 关闭当前不需要使用的配网管理器，释放配网相关资源。
        // 启动wifi 进入sta模式
        esp_wifi_set_mode(WIFI_MODE_STA); // 把 Wi-Fi 驱动模式设置为 Station，即设备作为热点客户端。
        esp_wifi_start(); // 启动驱动，随后 STA_START 事件会调用 esp_wifi_connect 发起连接。
    }

    ESP_LOGI(TAG, "wifi_init_sta finished."); // 记录 Wi-Fi 相关资源初始化完成，尚不表示已经取得 IP。

    ESP_LOGI(TAG, "主程序 正在等待wifi连接"); // 输出主流程即将阻塞等待联网的提示。

    // 等待获得 IP 才允许主流程继续访问 HTTPS 服务；pdFALSE 表示返回时不清事件位。
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, // 开始等待本模块事件组，函数返回的事件位保存到 bits。
                                           WIFI_CONNECTED_BIT, // 只等待 WIFI_CONNECTED_BIT，它在 STA_GOT_IP 事件中被置位。
                                           pdFALSE, // 第一个 pdFALSE 表示本次等待退出时不自动清除事件位。
                                           pdFALSE, // 第二个 pdFALSE 表示任一指定事件位就满足；此处仅指定了一个位。
                                           portMAX_DELAY); // portMAX_DELAY 表示持续等待，不对启动联网设置超时时间。

    ESP_LOGI(TAG, "整个wifi初始化流程 彻底结束了！而且已经有网络了！"); // 等待返回后记录联网流程完成，主程序可以继续访问激活服务器。
}

/**
 * @brief 恢复 Wi-Fi 驱动默认配置并立即重启设备，供下次启动重新配网。
 * 此处调用 esp_wifi_restore，不执行整片 Flash 擦除；函数正常情况下不会返回。
 */
void xiaozhi_wifi_reset(void)
{
    esp_wifi_restore(); // 把 Wi-Fi 驱动的持久化配置恢复为默认值，为重新配网清除旧网络设置。
    // 重启esp32单片机
    esp_restart(); // 立即软件重启，下次进入 wifi_init 时重新检查是否需要 BLE 配网。
}