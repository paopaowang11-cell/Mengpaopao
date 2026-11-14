/**
 * @file xiaozhi_lcd.c
 * ST7789 屏幕底层驱动：SPI 总线、屏幕控制和背光 GPIO 初始化。
 * 引脚和方向设置对应当前硬件连接；上层 xiaozhi_display.c 负责 LVGL 界面。
 */
#include "display/xiaozhi_lcd.h"

// 使用 SPI2 外设连接 LCD；本项目按单数据线 SPI 模式传输。
#define LCD_HOST SPI2_HOST
// 底层最大传输容量按 16 行像素预留；行数越多，占用内存越大。
// 这里用于计算 SPI 最大事务大小，上层 LVGL 绘图缓冲区大小另行配置。
#define PARALLEL_LINES 16
// 示例遗留的旋转帧数宏，当前显示流程未使用。
#define ROTATE_FRAME 30

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// 屏幕时钟、控制引脚与像素格式，请结合当前硬件接线理解 //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 以下参数对应当前 ST7789 接线：MOSI48、SCLK47、CS21、DC45、RST16、背光40。
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_DATA0 48 /*!< 单数据线 SPI 模式下的数据输出引脚 MOSI。 */
#define EXAMPLE_PIN_NUM_PCLK 47
#define EXAMPLE_PIN_NUM_CS 21
#define EXAMPLE_PIN_NUM_DC 45
#define EXAMPLE_PIN_NUM_RST 16
#define EXAMPLE_PIN_NUM_BK_LIGHT 40

// 屏幕横向与纵向像素数；最终显示方向还受坐标轴交换设置影响。
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 240
// LCD 命令及参数各占 8 bit。
#define EXAMPLE_LCD_CMD_BITS 8
#define EXAMPLE_LCD_PARAM_BITS 8

#define IMAGE_W 320
#define IMAGE_H 240

esp_lcd_panel_handle_t panel_handle = NULL;
/**
 * @brief 初始化 SPI2 总线和 ST7789 屏幕，配置方向并开启背光。
 * RGB565 每像素占 2 字节；SPI 时钟为 40 MHz，背光在初始化期间暂时关闭。
 * 本层执行坐标轴交换和 X 方向镜像，上层 LVGL 分辨率需与最终方向配合。
 */
void xiaozhi_lcd_init(void)
{
    gpio_config_t bk_gpio_config = { // 开始构造背光 GPIO 配置，只针对当前背光引脚生效。
        .mode = GPIO_MODE_OUTPUT, // 把背光引脚设置为输出模式，由 ESP32 控制亮灭电平。
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT}; // 将 64 位整数 1 左移到背光引脚号位置，生成对应 GPIO 位掩码。
    // 初始化背光控制 GPIO 为输出模式。
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config)); // 应用 GPIO 配置；ESP_ERROR_CHECK 在返回错误时终止当前正常执行流程。

    spi_bus_config_t buscfg = { // 定义 SPI 总线配置，包括时钟、数据线以及单次传输容量。
        .sclk_io_num = EXAMPLE_PIN_NUM_PCLK, // 将 SPI 时钟连接到配置的 PCLK 引脚 GPIO47。
        .mosi_io_num = EXAMPLE_PIN_NUM_DATA0, // 将 SPI 主机输出 MOSI 连接到 DATA0 引脚 GPIO48。
        .miso_io_num = -1, // 设为 -1 表示不用 MISO，本项目只向屏幕写数据。
        .quadwp_io_num = -1, // 不使用四线 SPI 的写保护扩展数据引脚。
        .quadhd_io_num = -1, // 不使用四线 SPI 的保持扩展数据引脚。
        // RGB565 每像素 2 字节，预留 16 行像素数据再加 8 字节传输余量。
        .max_transfer_sz = PARALLEL_LINES * EXAMPLE_LCD_H_RES * 2 + 8}; // 为 16 行×320 像素×2 字节及 8 字节余量预留最大传输空间。

    // 创建 SPI 总线并自动选择 DMA 通道。
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO)); // 初始化 SPI2 总线，自动选择 DMA 通道，并检查是否成功。

    esp_lcd_panel_io_handle_t io_handle = NULL; // 先把 LCD I/O 句柄设为空，下一步创建接口时通过地址写入结果。
    esp_lcd_panel_io_spi_config_t io_config = { // 定义屏幕 SPI I/O 的控制引脚、时钟、命令长度及队列配置。
        .dc_gpio_num = EXAMPLE_PIN_NUM_DC, // 用 GPIO45 区分当前传输的是命令还是像素/参数数据。
        .cs_gpio_num = EXAMPLE_PIN_NUM_CS, // 用 GPIO21 作为片选，选中对应 LCD SPI 设备。
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ, // 设置 LCD SPI 传输时钟为 40 MHz，单位是 Hz。
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS, // 每条 LCD 命令使用 8 bit。
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS, // 每个 LCD 参数使用 8 bit。
        // SPI 模式 0，队列深度 10 表示可排队的事务数量。
        .spi_mode = 0, // 选择 SPI 模式 0，即对应的时钟极性与采样相位组合。
        .trans_queue_depth = 10, // 允许传输队列最多容纳 10 个事务。
    };

    // 创建 LCD 的 SPI I/O 接口，绑定片选、命令/数据控制及时钟配置。
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle)); // 在 SPI2 上创建面板 I/O，保存 io_handle，并检查创建结果。

    esp_lcd_panel_dev_config_t panel_config = { // 定义 LCD 面板本身的复位引脚、颜色顺序及像素位宽。
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST, // 指定 GPIO16 作为硬件复位信号引脚。
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // 指定 RGB 元素顺序，避免把 RGB/BGR 顺序与字节序混淆。
        .bits_per_pixel = 16, // 每个像素使用 16 bit，匹配 RGB565 和传输容量计算。
    };
    // 使用上述 I/O 接口创建 ST7789 面板实例。
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle)); // 创建 ST7789 面板实例，把句柄写入全局 panel_handle。

    // 面板复位和初始化期间先关闭背光，减少初始化画面闪烁。
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL)); // 将背光设为关闭电平，让屏幕初始化过程暂时不可见。

    // 通过配置的复位引脚复位屏幕。
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle)); // 根据面板驱动和复位引脚配置执行屏幕复位。

    // 写入驱动所需的面板初始化配置。
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle)); // 发送 ST7789 初始化配置，使屏幕进入可显示状态。

    // 开启面板显示输出。
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true)); // 开启面板显示输出；背光仍由单独 GPIO 控制。
    // ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    // 交换 X/Y 坐标轴，配合上层逻辑分辨率调整屏幕方向。
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true)); // 交换屏幕 X/Y 坐标轴，配合上层 240×320 逻辑分辨率。

    // 开启背光：当前电路使用高电平点亮。
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL)); // 把背光 GPIO 设为开启电平，使屏幕内容可见。

    // 在交换坐标轴后再配置镜像，使显示方向符合当前屏幕安装方式。
    esp_lcd_panel_mirror(panel_handle, true, false); // 开启 X 方向镜像、关闭 Y 方向镜像，使图像符合当前安装方向。
}
