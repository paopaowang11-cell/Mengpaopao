/**
 * @file xiaozhi_display.c
 * LVGL 界面层，提供提示、字幕、表情和配网二维码显示接口。
 * 业务可能从多个任务调用显示接口，通过 lvgl_port_lock/unlock 保护 LVGL 对象。
 */
#include "display/xiaozhi_display.h"
#include "esp_lvgl_port.h"
#include "display/xiaozhi_lcd.h"
#include "lv_demos.h"
#include "esp_log.h"
// #include "font_puhui.h"
#include "font_emoji.h"
#include "common_types.h"

#define TAG "xiaozhi_display"

#define LCD_H_RES 320
#define LCD_V_RES 240

#define LCD_DRAW_BUFF_HEIGHT 10

static lv_display_t *lvgl_disp = NULL;

// 对象句柄在界面初始化/二维码创建时赋值，后续显示接口通过这些句柄更新对象。
lv_obj_t *qr;
lv_obj_t *text_label;
lv_obj_t *emoji_label;
lv_obj_t *tip_label;

extern const lv_font_t font_puhui_20_4;

// 左侧名称必须与服务器 llm/emotion 字段对应，右侧为字库支持的 UTF-8 表情。
const xiaozhi_emoji_t emoji_list[21] = {
    {.name = "neutral", .emoji = "😶"},
    {.name = "happy", .emoji = "🙂"},
    {.name = "laughing", .emoji = "😆"},
    {.name = "funny", .emoji = "😂"},
    {.name = "sad", .emoji = "😔"},
    {.name = "angry", .emoji = "😠"},
    {.name = "crying", .emoji = "😭"},
    {.name = "loving", .emoji = "😍"},
    {.name = "embarrassed", .emoji = "😳"},
    {.name = "surprised", .emoji = "😲"},
    {.name = "shocked", .emoji = "😱"},
    {.name = "thinking", .emoji = "🤔"},
    {.name = "winking", .emoji = "😉"},
    {.name = "cool", .emoji = "😎"},
    {.name = "relaxed", .emoji = "😌"},
    {.name = "delicious", .emoji = "🤤"},
    {.name = "kissy", .emoji = "😘"},
    {.name = "confident", .emoji = "😏"},
    {.name = "sleepy", .emoji = "😴"},
    {.name = "silly", .emoji = "😜"},
    {.name = "confused", .emoji = "🙄"}};

/**
 * @brief 初始化 LCD 与 LVGL 移植层，创建提示、表情和字幕三个标签。
 * LVGL 逻辑分辨率为 240×320，RGB565 双缓冲，单缓冲 320×10 个像素。
 * 中文字库显示提示/字幕，表情字库显示 emoji；所有对象操作放在 LVGL 锁内。
 */
void xiaozhi_display_init(void)
{
    // 1. 横竖要颠倒过来
    // 2. rgb相关的配置都不能开
    // 3. swap_bytes 不开颜色会翻转 所以需要开启。

    xiaozhi_lcd_init(); // 先初始化 ST7789、SPI 和背光，获得可供 LVGL 使用的面板句柄。

    /* 初始化 LVGL 移植层的刷新任务和周期 tick。 */
    const lvgl_port_cfg_t lvgl_cfg = { // 定义 LVGL 移植层后台刷新任务与 tick 定时器的参数。
        .task_priority = 4, // 设置 LVGL 刷新任务优先级为 4。
        .task_stack = 6144, // 为 LVGL 刷新任务配置 6144 字节栈空间。
        .task_affinity = -1, // -1 表示刷新任务不固定在某个 CPU 核心。
        .task_max_sleep_ms = 500, // 刷新任务一次最多休眠 500 ms，期间也可被相关事件唤醒。
        .timer_period_ms = 5 // 设置 LVGL tick 定时周期为 5 ms，为动画和定时处理提供时间基准。
    };
    lvgl_port_init(&lvgl_cfg); // 启动 LVGL 移植层，创建所需任务、定时器和互斥锁等资源。

    // buffer_size 以像素计；320×10×2=6400 字节/缓冲，双缓冲像素存储约 12.5 KiB。
    uint32_t buff_size = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT; // 计算单块绘图缓冲的像素数：320×10=3200；此变量不是字节长度。

    /* 将底层 LCD 面板加入 LVGL 显示设备列表。 */
    ESP_LOGD(TAG, "Add LCD screen"); // 输出添加屏幕设备的调试日志。
    const lvgl_port_display_cfg_t disp_cfg = { // 定义 LVGL 显示设备配置，把底层面板和绘图缓冲组合起来。
        .panel_handle = panel_handle, // 绑定底层 ST7789 初始化得到的面板句柄。
        .buffer_size = buff_size, // 单块绘图缓冲容量设为 3200 像素，RGB565 下为 6400 字节。
        .double_buffer = true, // 启用双缓冲，让绘制与传输可以使用不同缓冲区。
        // 与底层横纵轴交换配合，LVGL 使用宽 240、高 320 的逻辑坐标。
        .hres = LCD_V_RES, // 逻辑宽度使用 LCD_V_RES，即 240，配合底层坐标轴交换。
        .vres = LCD_H_RES, // 逻辑高度使用 LCD_H_RES，即 320。
        .monochrome = false, // 声明使用彩色屏幕，不采用单色像素格式。
        .color_format = LV_COLOR_FORMAT_RGB565, // 像素格式为 RGB565，红/绿/蓝分别占 5/6/5 bit。
        .rotation = { // 开始填写 LVGL 层的显示旋转参数。
            .swap_xy = false, // 本层不再交换 X/Y，轴交换已经在底层面板中配置。
            .mirror_x = false, // 本层不追加 X 轴镜像，避免重复叠加方向变换。
            .mirror_y = false, // 本层不追加 Y 轴镜像。
        },
        .flags = { // 开始填写绘图缓冲及颜色传输相关标志。
            .buff_dma = true, // 要求使用适合 DMA 传输的绘图缓冲配置。
            .buff_spiram = true, // 请求把绘图缓冲分配到 PSRAM，减轻内部 RAM 占用。
            // 交换 RGB565 的字节顺序以适配屏幕传输格式，不代表交换 RGB/BGR 颜色通道。
            .swap_bytes = true, // 交换 RGB565 每个像素的两个字节，以匹配屏幕传输字节序。

        }};
    const lvgl_port_display_rgb_cfg_t rgb_cfg = { // 定义当前调用 add_disp_rgb 所使用的扩展显示参数。
        .flags = { // 开始设置该扩展配置的模式标志。
            .bb_mode = false, // 关闭 RGB bounce buffer 模式，保留当前移植层调用配置。
            .avoid_tearing = false, // 关闭该接口提供的防撕裂模式，保留当前显示方案。
        }};
    lvgl_disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg); // 根据面板和扩展配置注册显示设备，并保存返回的 LVGL 显示句柄。

    // 1. 创建一个专门用来显示字幕的区域
    // 跨任务操作 LVGL 对象前先加锁；参数 0 在此移植层表示无限等待锁。
    lvgl_port_lock(0); // 取得 LVGL 递归互斥锁；0 表示一直等待，确保创建对象时不会并发刷新访问。
    lv_obj_t *screen = lv_screen_active(); // 获取当前活动屏幕，后面的标签都以它为父对象。
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xD4D6DC), LV_PART_MAIN); // 把屏幕主体背景设为十六进制颜色 D4D6DC 的浅灰色。

    // 1.1 创建一个提示标签
    tip_label = lv_label_create(screen); // 在屏幕上创建顶部提示标签，并保存句柄供后续更新。
    // 1.2 设置文本内容
    lv_label_set_text(tip_label, "看见朕还不给朕点赞"); // 写入启动时的默认提示文字，LVGL 会复制该字符串。
    // 1.3 设置标签的宽度为240 刚好跟屏幕一样宽
    lv_obj_set_style_width(tip_label, 240, LV_PART_MAIN); // 将提示标签宽度设为 240 像素，与逻辑屏幕宽度一致。
    // 1.4 让整个标签文本框对到屏幕的正中间
    lv_obj_align(tip_label, LV_ALIGN_TOP_MID, 0, 0); // 把提示标签顶部居中，X/Y 偏移均为 0。
    // 1.5 设置标签的文本颜色为黑色
    lv_obj_set_style_text_color(screen, lv_color_hex(0x000000), LV_PART_MAIN); // 在 screen 上设置主体文字颜色为黑色，未覆盖的子对象可继承此样式。
    // 1.6 标签中的文字居中对齐
    lv_obj_set_style_text_align(tip_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN); // 设置提示标签内部文字水平居中，不等同于移动标签位置。
    // 1.7 为这个标签挂载中文字库
    lv_obj_set_style_text_font(tip_label, &font_puhui_20_4, LV_PART_MAIN); // 为提示标签设置普惠 20 号中文字库，支持中文提示绘制。
    // 1.8 设置标签的背景色为纯白色
    lv_obj_set_style_bg_color(tip_label, lv_color_hex(0xffffff), LV_PART_MAIN); // 将提示标签背景设为白色。
    // 1.9 设置为不透明
    lv_obj_set_style_bg_opa(tip_label, LV_OPA_COVER, LV_PART_MAIN); // 把背景不透明度设为完全不透明，使白色提示背景实际可见。

    // 2.0 创建一个表情标签
    emoji_label = lv_label_create(screen); // 创建独立的表情标签，供服务端 emotion 消息更新。
    // 2.1 设置一个表情
    lv_label_set_text(emoji_label, "🙂"); // 设置开机默认表情为微笑。
    // 2.2 这个表情以屏幕的正中心为基准，向上偏移70像素
    lv_obj_align(emoji_label, LV_ALIGN_CENTER, 0, -70); // 以屏幕中心为基准，把表情向上移动 70 像素。
    // 2.3 设置标签中文字（表情）的对齐方式 让它放在标签的正中间
    lv_obj_set_style_text_align(emoji_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN); // 让表情在其标签区域中居中排列。
    // 2.4 挂在表情包字库
    lv_obj_set_style_text_font(emoji_label, font_emoji_64_init(), LV_PART_MAIN); // 初始化并设置 64 像素表情字库，使 Unicode emoji 可被绘制。

    // 3.0 创建屏幕中间显示字幕的文本框
    text_label = lv_label_create(screen); // 创建用于字幕和激活提示的文字标签。
    // 3.1 设置文本框中的文字内容
    lv_label_set_text(text_label, "Meng跑跑AI小智项目"); // 设置项目启动时默认显示的字幕文字。
    // 3.2 设置文本框240像素宽
    lv_obj_set_style_width(text_label, 240, LV_PART_MAIN); // 将字幕区域宽度设为 240 像素，较长文本可在此宽度内布局。
    // 3.3 让这个文本框显示在表情包的正下方 并且保持10像素的间距(这个地方必须先设宽度 再对齐)
    lv_obj_align_to(text_label, emoji_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10); // 把字幕放在表情标签下方并居中，垂直间距为 10 像素。
    // 3.4 让这个文本框中的文字为黑色
    lv_obj_set_style_text_color(screen, lv_color_hex(0x000000), LV_PART_MAIN); // 再次将屏幕的主体文字颜色设为黑色，供子对象继承。
    // 3.5 这个文本框中的文字全局居中
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN); // 让字幕文字在标签范围内水平居中。
    // 3.6 挂在中文字库
    lv_obj_set_style_text_font(text_label, &font_puhui_20_4, LV_PART_MAIN); // 为字幕标签设置中文字库，以显示服务器字幕和中文提示。

    lvgl_port_unlock(); // 全部对象创建完后释放 LVGL 锁，允许刷新任务继续访问界面。
}

/**
 * @brief 更新屏幕顶部提示文字，内部取得 LVGL 锁。
 * @param tip 以空字符结尾的 UTF-8 文本；lv_label_set_text 会复制字符串。
 */
void xiaozhi_display_tip(char *tip)
{
    lvgl_port_lock(0); // 无限等待并取得 LVGL 锁，避免其他任务同时改动界面对象。
    lv_label_set_text(tip_label, tip); // 更新顶部标签为 tip 的内容，LVGL 在内部复制以空字符结束的字符串。
    lvgl_port_unlock(); // 释放 LVGL 锁，让刷新任务能够绘制更新后的提示。
}
/**
 * @brief 更新表情下方的字幕或业务提示文字。
 * @param text 以空字符结尾的 UTF-8 文本；函数内加锁并由 LVGL 复制内容。
 * 调用方可在返回后释放临时字符串，例如激活码提示缓冲区。
 */
void xiaozhi_display_text(char *text)
{
    lvgl_port_lock(0); // 取得 LVGL 锁，保护字幕标签操作与界面刷新之间的并发访问。
    lv_label_set_text(text_label, text); // 将 text 复制为字幕标签内容，调用返回后原文本缓冲可由调用方释放。
    lvgl_port_unlock(); // 完成更新后释放 LVGL 锁。
}

/**
 * @brief 将服务器表情名称转换成对应 emoji 并更新标签。
 * @param emoji_name 与 emoji_list 中 name 对应的字符串，例如 happy、thinking。
 * 顺序搜索 21 项映射，未匹配时显示默认表情；调用前须完成界面初始化。
 */
void xiaozhi_display_emoji(char *emoji_name)
{
    lvgl_port_lock(0); // 取得 LVGL 锁，保证表情查找后对标签的修改处于受保护状态。
    bool found = false; // 先假设没有匹配项，用于决定最后是否显示默认表情。
    for (uint8_t i = 0; i < 21; i++) // 依次遍历 21 个名称与表情映射，i 从 0 增长到 20。
    {
        if (strcmp(emoji_list[i].name, emoji_name) == 0) // 逐项比较名称；strcmp 返回 0 时表示与服务器 emotion 名称一致。
        {
            lv_label_set_text(emoji_label, emoji_list[i].emoji); // 把匹配项的 UTF-8 emoji 字符串写入表情标签。
            found = true; // 记录已经找到匹配项，避免循环后又覆盖为默认表情。
            break; // 找到第一项后立即跳出 for 循环，无需继续比较其余映射。
        }
    }
    if (found == false) // 遍历结束仍未找到对应名称时，执行默认表情分支。
    {
        lv_label_set_text(emoji_label, "😏"); // 显示当前约定的默认表情，保证未知名称也有可见结果。
    }

    lvgl_port_unlock(); // 标签处理完成后释放 LVGL 锁。
}

// void xiaozhi_display_test(void)
// {
//     lvgl_port_lock(0);
//     lv_obj_t *screen = lv_screen_active();

//     lv_obj_set_style_bg_color(screen, lv_color_hex(0x003a57), LV_PART_MAIN);

//     lv_obj_t *text_label = lv_label_create(screen);
//     lv_label_set_text(text_label, "Hello World!");
//     lv_obj_set_style_bg_color(text_label, lv_color_hex(0xFBC778), LV_PART_MAIN);
//     lv_obj_set_style_text_color(screen, lv_color_hex(0xffffff), LV_PART_MAIN);
//     lv_obj_set_style_bg_opa(text_label, LV_OPA_COVER, LV_PART_MAIN);
//     lv_obj_align(text_label, LV_ALIGN_CENTER, 0, 0);

//     lvgl_port_unlock();
// }

/**
 * @brief 在当前屏幕中央创建 220 像素二维码，用于手机扫描配网。
 * @param data 二维码原始数据，本项目传入配网 JSON 载荷。
 * @param data_len 有效数据的字节数，不包含字符串结尾的空字符。
 * 保存二维码对象句柄 qr 供联网后删除；当前流程按每次配网创建一个对象使用。
 */
void xiaozhi_display_show_qrcode(char *data, uint32_t data_len)
{
    lvgl_port_lock(0); // 取得 LVGL 锁，保护接下来创建和配置二维码对象的操作。

    lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5); // 从浅蓝调色板取得更浅的颜色，作为二维码背景及边框颜色。
    lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4); // 从蓝色调色板取得较深的颜色，作为二维码有效图形颜色。

    qr = lv_qrcode_create(lv_screen_active()); // 在活动屏幕上创建二维码对象，并把句柄保存在 qr 中。
    lv_qrcode_set_size(qr, 220); // 设置二维码区域边长为 220 像素。
    lv_qrcode_set_dark_color(qr, fg_color); // 设置二维码深色模块的绘制颜色。
    lv_qrcode_set_light_color(qr, bg_color); // 设置二维码浅色模块的背景颜色。

    /* 把配网数据编码为二维码像素内容。 */

    lv_qrcode_update(qr, data, data_len); // 按 data_len 字节把 data 编码成二维码内容，不把末尾空字符算进去。
    lv_obj_center(qr); // 把二维码对象放在屏幕正中央。

    /* 使用背景色增加 5 像素边框，给二维码边缘留出识别空间。 */
    lv_obj_set_style_border_color(qr, bg_color, 0); // 将二维码边框颜色设为与浅色背景一致。
    lv_obj_set_style_border_width(qr, 5, 0); // 设置 5 像素边框，为码图四周留出识别空间。

    lvgl_port_unlock(); // 释放 LVGL 锁，允许刷新任务显示二维码。
}

/**
 * @brief 删除配网二维码并把句柄清空；尚未创建时直接返回。
 * 删除 LVGL 对象时持有移植层互斥锁，避免与界面刷新任务并发访问。
 */
void xiaozhi_display_delete_qrcode(void)
{
    if (qr == NULL) // 先判断是否存在二维码对象，避免对空句柄执行删除。
    {
        return; // 没有二维码时直接返回，使重复调用不会再次删除同一个对象。
    }
    lvgl_port_lock(0); // 取得 LVGL 锁，防止删除对象时刷新任务仍访问它。
    lv_obj_del(qr); // 删除二维码及其关联的 LVGL 对象资源。
    qr = NULL; // 将保存的句柄清空，避免后续把已失效地址继续当作有效对象。
    lvgl_port_unlock(); // 释放 LVGL 锁，结束此次界面对象操作。
}