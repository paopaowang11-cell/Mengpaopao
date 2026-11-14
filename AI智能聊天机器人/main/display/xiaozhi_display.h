/**
 * @file xiaozhi_display.h
 * LVGL 界面接口：提示、字幕、表情与配网二维码。
 * 接口的调用顺序、参数单位与数据生命周期见各声明处说明。
 */
#pragma once

#include <stdint.h>

/**
 * @brief 保留的界面测试接口声明。
 * 对应测试实现在 .c 中已整段注释，当前没有可链接的函数定义，业务代码未调用。
 */
void xiaozhi_display_test(void);

/**
 * @brief 初始化 LCD 与 LVGL 移植层，创建提示、表情和字幕三个标签。
 * LVGL 逻辑分辨率为 240×320，RGB565 双缓冲，单缓冲 320×10 个像素。
 * 中文字库显示提示/字幕，表情字库显示 emoji；所有对象操作放在 LVGL 锁内。
 */
void xiaozhi_display_init(void);

/**
 * @brief 在当前屏幕中央创建 220 像素二维码，用于手机扫描配网。
 * @param data 二维码原始数据，本项目传入配网 JSON 载荷。
 * @param data_len 有效数据的字节数，不包含字符串结尾的空字符。
 * 保存二维码对象句柄 qr 供联网后删除；当前流程按每次配网创建一个对象使用。
 */
void xiaozhi_display_show_qrcode(char *data, uint32_t data_len);

/**
 * @brief 删除配网二维码并把句柄清空；尚未创建时直接返回。
 * 删除 LVGL 对象时持有移植层互斥锁，避免与界面刷新任务并发访问。
 */
void xiaozhi_display_delete_qrcode(void);

/**
 * @brief 更新表情下方的字幕或业务提示文字。
 * @param text 以空字符结尾的 UTF-8 文本；函数内加锁并由 LVGL 复制内容。
 * 调用方可在返回后释放临时字符串，例如激活码提示缓冲区。
 */
void xiaozhi_display_text(char *text);

/**
 * @brief 将服务器表情名称转换成对应 emoji 并更新标签。
 * @param emoji_name 与 emoji_list 中 name 对应的字符串，例如 happy、thinking。
 * 顺序搜索 21 项映射，未匹配时显示默认表情；调用前须完成界面初始化。
 */
void xiaozhi_display_emoji(char *emoji_name);

/**
 * @brief 更新屏幕顶部提示文字，内部取得 LVGL 锁。
 * @param tip 以空字符结尾的 UTF-8 文本；lv_label_set_text 会复制字符串。
 */
void xiaozhi_display_tip(char *tip);
