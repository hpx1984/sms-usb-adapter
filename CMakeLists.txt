cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH 环境变量指向 pico-sdk
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(sms_usb_adapter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(sms_usb_adapter
    main.c
    tusb_config.h
)

target_include_directories(sms_usb_adapter PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(sms_usb_adapter
    pico_stdlib
    pico_stdio_uart
    tinyusb_host
)
pico_enable_stdio_usb(sms_usb_adapter 0)   # USB 口给 Host 用, 不做串口
pico_enable_stdio_uart(sms_usb_adapter 1)
pico_add_extra_outputs(sms_usb_adapter)    # 生成 .uf2
