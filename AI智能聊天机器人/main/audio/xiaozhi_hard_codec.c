/**
 * @file xiaozhi_hard_codec.c
 * ES8311 硬件音频适配层：I2C 配置、I2S 录放音和音量控制。
 * 本模块处理 PCM，与负责压缩码流的 Opus 软件编解码器分工不同。
 */
#include "audio/xiaozhi_hard_codec.h"

// 总线、通道和 codec 句柄在初始化时创建，供录音/播放任务长期复用。
i2c_master_bus_handle_t i2c0_bus_handle;

i2s_chan_handle_t i2s_tx_chan_handle;
i2s_chan_handle_t i2s_rx_chan_handle;

esp_codec_dev_handle_t codec_dev_handle;

/**
 * @brief 创建 I2C0 主机总线，供 ES8311 寄存器配置使用。
 * SCL=GPIO1，SDA=GPIO0；启用内部上拉，glitch_ignore_cnt=7 用于滤除短毛刺。
 */
void xiaozhi_hard_codec_i2c_init(void)
{
    i2c_master_bus_config_t i2c_bus_config = {0}; // 创建并清零 I2C 总线配置，未显式设置的成员保持默认值。
    i2c_bus_config.i2c_port = I2C_NUM_0; // 选择 I2C0 控制器，用于访问 ES8311 寄存器。
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT; // 使用驱动默认时钟源，不手动指定底层时钟。

    i2c_bus_config.scl_io_num = GPIO_NUM_1; // 把串行时钟 SCL 连接到 GPIO1。
    i2c_bus_config.sda_io_num = GPIO_NUM_0; // 把双向数据线 SDA 连接到 GPIO0。
    i2c_bus_config.glitch_ignore_cnt = 7; // 过滤小于 7 个 I2C 模块时钟周期的毛刺。
    i2c_bus_config.flags.enable_internal_pullup = true; // 启用引脚内部上拉，帮助 I2C 总线维持空闲高电平。
    i2c_new_master_bus(&i2c_bus_config, &i2c0_bus_handle); // 依据配置创建主机总线，把新句柄写入 i2c0_bus_handle。
}

/**
 * @brief 创建并启用 I2S0 全双工标准音频通道。
 * 采用 Philips 格式、16 kHz、16 bit、单声道；TX 播放、RX 录音，共用时钟。
 * MCLK/BCLK/WS/DOUT/DIN 分别连接 GPIO3/2/5/6/4，数据方向以 ESP32 为参照。
 */
void xiaozhi_hard_codec_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER); // 获取 I2S0 主机模式的默认通道配置，由 ESP32 提供音频时钟。
    // TX 发送结束后自动清理 DMA 缓冲区，减少无新音频时残留数据重复输出。
    chan_cfg.auto_clear = true; // 发送回调后自动清零 TX DMA 缓冲，无新数据时输出零而非旧音频。
    i2s_std_config_t std_cfg = { // 开始定义标准 I2S 的时钟、时隙与引脚配置。
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000), // 配置采样率为 16000 Hz，即每秒 16000 个采样点。
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_MONO), // 使用 Philips I2S 格式，采样位宽为 16 bit，逻辑音频为单声道。
        .gpio_cfg = { // 开始填写音频总线各信号对应的 GPIO。
            .mclk = GPIO_NUM_3, // GPIO3 输出主时钟 MCLK，供 ES8311 工作使用。
            .bclk = GPIO_NUM_2, // GPIO2 输出位时钟 BCLK，用来同步每一位音频数据。
            .ws = GPIO_NUM_5, // GPIO5 输出字选择/帧同步信号 WS。
            .dout = GPIO_NUM_6, // GPIO6 是 ESP32 的音频数据输出端，连接 ES8311 播放输入。
            .din = GPIO_NUM_4}, // GPIO4 是 ESP32 的音频数据输入端，接收 ES8311 录音数据。
    };

    // 创建通道
    i2s_new_channel(&chan_cfg, &i2s_tx_chan_handle, &i2s_rx_chan_handle); // 同时创建 TX 和 RX 通道，将句柄分别保存到两个全局变量。
    // 初始化通道
    i2s_channel_init_std_mode(i2s_tx_chan_handle, &std_cfg); // 把 TX 通道配置成上述标准 I2S 模式，用于播放。
    i2s_channel_init_std_mode(i2s_rx_chan_handle, &std_cfg); // 把 RX 通道配置成相同格式，用于录音。
    // 使能这两个通道
    i2s_channel_enable(i2s_tx_chan_handle); // 使能发送通道，使其进入可传输音频的状态。
    i2s_channel_enable(i2s_rx_chan_handle); // 使能接收通道，使其进入可采集音频的状态。
}

/**
 * @brief 初始化 ES8311 硬件与 esp_codec_dev 录放音抽象设备。
 * I2C 接口负责控制寄存器，I2S 接口负责 PCM 数据，GPIO7 控制外部功放。
 * 打开 16 kHz、16 bit、单声道录放音，设置初始输出音量 90 和输入增益 15 dB。
 * 由 xiaozhi_sr_init 调用，应在开始录音、解码播放或远程音量控制前完成。
 */
void xiaozhi_hard_codec_init(void)
{
    xiaozhi_hard_codec_i2c_init(); // 先初始化 I2C 控制总线，为后续配置 ES8311 寄存器做准备。
    xiaozhi_hard_codec_i2s_init(); // 再初始化 I2S 全双工通道，建立 PCM 录放音通路。

    // 为编解码器设备实现控制接口，数据接口和 GPIO 接口 (使用默认提供的接口实现)
    // 数据接口封装 I2S 的全双工通道，后续 read/write 不必直接操作底层通道。
    audio_codec_i2s_cfg_t i2s_cfg = { // 构建 esp_codec_dev 所需的 I2S 数据接口配置。
        .rx_handle = i2s_rx_chan_handle, // 将录音数据接口绑定到此前创建的 RX 通道。
        .tx_handle = i2s_tx_chan_handle}; // 将播放数据接口绑定到此前创建的 TX 通道。

    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg); // 根据 I2S 配置创建数据接口对象，供统一 read/write 接口使用。

    // 控制接口使用 ES8311 默认 I2C 地址，经 I2C0 访问设备寄存器。
    audio_codec_i2c_cfg_t i2c_cfg = { // 构建音频芯片寄存器访问所需的 I2C 控制配置。
        .addr = ES8311_CODEC_DEFAULT_ADDR, // 使用组件提供的 ES8311 默认 I2C 地址宏。
        .bus_handle = i2c0_bus_handle}; // 指定通过刚创建的 I2C0 总线访问芯片。
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg); // 创建 I2C 控制接口，用于读写音频芯片寄存器。

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio(); // 创建默认 GPIO 接口，用于控制功放等相关引脚。

    // 基于控制接口和 ES8311 特有的配置实现 audio_codec_if_t 接口

    es8311_codec_cfg_t es8311_cfg = { // 构建 ES8311 芯片专用的工作模式及外设接口配置。
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH, // 同时开启芯片的输入与输出功能，即录音和播放。
        .ctrl_if = out_ctrl_if, // 将 ES8311 的寄存器操作绑定到 I2C 控制接口。
        .gpio_if = gpio_if, // 将芯片相关引脚操作绑定到默认 GPIO 接口。
        .pa_pin = GPIO_NUM_7, // 指定 GPIO7 为外部功放控制引脚。
        .use_mclk = true}; // 启用外部 MCLK，使用 ESP32 输出的主时钟。
    const audio_codec_if_t *out_codec_if = es8311_codec_new(&es8311_cfg); // 根据专用配置创建 ES8311 编解码芯片接口实例。

    esp_codec_dev_cfg_t dev_cfg = { // 把芯片控制接口与音频数据接口组合成统一设备配置。
        .codec_if = out_codec_if, // 设置底层芯片操作接口为刚创建的 ES8311 实例。
        .data_if = data_if, // 设置音频数据传输接口为前面创建的 I2S 数据接口。
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT, // 声明这是同时支持输入、输出的音频设备。
    };
    codec_dev_handle = esp_codec_dev_new(&dev_cfg); // 创建统一音频设备，把句柄保存给录放音及音量接口复用。
    // 设置录放音默认参数；播放入口为下面的 xiaozhi_hard_codec_play。
    esp_codec_dev_set_out_vol(codec_dev_handle, 90.0); // 把初始输出音量设为 90；此调用不会更新下面的 last_volume。
    esp_codec_dev_set_in_gain(codec_dev_handle, 15.0); // 把麦克风输入增益设为 15 dB。

    esp_codec_dev_sample_info_t fs = { // 定义打开设备时使用的 PCM 采样格式。
        .sample_rate = 16000, // PCM 采样率设为 16000 Hz，与 AFE 和编解码器配置一致。
        .channel = 1, // PCM 通道数设为 1，即单声道。
        .bits_per_sample = 16, // 每个采样值使用 16 bit，也就是 2 字节。
    };
    esp_codec_dev_open(codec_dev_handle, &fs); // 按给定采样格式打开音频设备，使录放音接口可以工作。
}

/**
 * @brief 把 PCM 字节数据写入音频设备，经 I2S 送往 ES8311 播放。
 * @param data PCM 缓冲区，格式应与设备打开时的 16 kHz/16 bit/单声道一致。
 * @param data_size 本次写入的字节数，不是采样点数；底层写入可能等待音频传输。
 */
void xiaozhi_hard_codec_play(uint8_t *data, uint32_t data_size)
{
    esp_codec_dev_write(codec_dev_handle, data, data_size); // 将 data 指向的 data_size 字节 PCM 写入设备，由 I2S 送往扬声器播放。
}

/**
 * @brief 从 ES8311 录音通道读取 PCM 数据到调用方缓冲区。
 * @param data 调用方提供的可写缓冲区，至少容纳 data_size 字节。
 * @param data_size 请求读取的字节数；16 bit 单声道每个采样点占 2 字节。
 */
void xiaozhi_hard_codec_record(uint8_t *data, uint32_t data_size)
{
    esp_codec_dev_read(codec_dev_handle, data, data_size); // 从音频输入设备读取 data_size 字节 PCM，写到调用方提供的 data 缓冲区。
}

// 记录最近一次 set_volume 的参数；静音操作不会覆盖该记忆值。
int last_volume = 0;

/**
 * @brief 设置输出音量，并记住此值用于取消静音时恢复。
 * @param volume 协议约定为 0～100 的音量值；当前封装直接传给驱动，不额外限幅。
 */
void xiaozhi_hard_codec_set_volume(int volume)
{
    last_volume = volume; // 记住本次音量值，供之后取消静音时恢复。
    esp_codec_dev_set_out_vol(codec_dev_handle, volume); // 把同一个音量值传给音频驱动，立即调整实际输出音量。
}

/**
 * @brief 通过调节输出音量实现静音或恢复音量。
 * @param mute true 设置音量为 0，false 恢复 last_volume。
 * last_volume 由 set_volume 更新，初值为 0，不会自动记录初始化时设置的音量 90。
 */
void xiaozhi_hard_codec_set_mute(bool mute)
{
    esp_codec_dev_set_out_vol(codec_dev_handle, mute == true ? 0 : last_volume); // 三目运算选择目标音量：mute 为真时用 0，否则用最近记住的 last_volume。
}