# 注册 main 应用组件：SRCS 列出业务源码，INCLUDE_DIRS 指定编译时头文件搜索目录。
# pre_data 下的 .txt 被 WebSocket 模块作为 C 字符串包含，无需作为独立源码编译。
idf_component_register(SRCS "main.c" "./button/xiaozhi_button.c" "./internet/xiaozhi_wifi.c" "./display/xiaozhi_lcd.c" "./display/xiaozhi_display.c" "./internet/xiaozhi_active.c" "./audio/xiaozhi_hard_codec.c" "./audio/xiaozhi_sr.c" "./audio/xiaozhi_opus_encoder.c" "./audio/xiaozhi_opus_decoder.c" "./internet/xiaozhi_ws.c"
                    INCLUDE_DIRS "." "./button" "./internet" "./display" "./common" "./audio" "./pre_data")