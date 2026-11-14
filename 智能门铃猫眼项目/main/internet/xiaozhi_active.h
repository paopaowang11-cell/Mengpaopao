/**
 * @file xiaozhi_active.h
 * 设备激活查询接口：请求构造、UUID 持久化与响应解析。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "nvs_flash.h"
#include "nvs.h"

/**
 * @brief 向激活/OTA 查询接口发送同步 HTTPS POST，获取设备业务连接信息。
 * 先设置设备身份请求头和 JSON 请求体，perform 期间由事件回调接收并解析响应。
 * 此函数查询激活状态与连接参数，没有执行固件下载或 OTA 分区切换。
 * 应在 Wi-Fi 获取 IP 后、WebSocket 初始化前调用。
 */
void xiaozhi_active_request(void);

/**
 * @brief 为已创建的 HTTP 客户端设置 JSON 类型、固件标识和设备身份。
 * Device-Id 为 STA MAC，Client-Id 为从 NVS 读取或首次生成的 UUID。
 * 全局句柄引用静态字符数组，供后续 WebSocket 握手重复使用。
 */
void xiaozhi_active_set_request_header(void);

/**
 * @brief 优先读取 NVS 中保存的 UUID，不存在时生成并写入持久化存储。
 * @param uuid_str 输出缓冲区，至少 37 字节，容纳 36 个字符和结尾空字符。
 * 使用 xiaozhi_data 命名空间的 uuid 键；随机数格式化时设置 UUID v4 版本和变体位。
 */
void xiaozhi_active_uuid_v4_gen(char *uuid_str);

/**
 * @brief 构建 application 和 board 两部分 JSON，设置为 HTTP POST 正文。
 * application 包含版本及 ELF 摘要；board 的 SSID/RSSI/信道/IP 当前为固定示例值。
 * set_post_field 保存正文指针，请求执行前不能释放打印得到的 res 字符串。
 * cJSON_Delete 释放节点树，不会释放由 cJSON_PrintUnformatted 单独分配的 res。
 */
void xiaozhi_active_set_request_body(void);

/**
 * @brief 解析激活查询响应，把 WebSocket URL、令牌及激活状态保存到业务句柄。
 * @param res 响应 JSON 的起始地址。
 * @param len JSON 的有效字节数。
 * URL、token 和激活码复制到独立堆内存，避免后续使用 HTTP 接收缓冲区的悬空指针。
 * 按当前协议约定：含 activation 表示仍需激活；无此字段表示已激活。
 * 激活码缓冲区固定为 7 字节，当前实现按 6 位激活码加结尾空字符处理。
 */
void xiaozhi_active_parse_response(char *res, int len);