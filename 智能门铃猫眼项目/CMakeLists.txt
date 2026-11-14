# 工程级构建入口：要求安装 ESP-IDF，并通过 IDF_PATH 定位其 CMake 构建系统。
# 先声明最低 CMake 版本，再引入 project.cmake，最后定义工程名称；保持此顺序。
# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(atguigu_xiaozhi)