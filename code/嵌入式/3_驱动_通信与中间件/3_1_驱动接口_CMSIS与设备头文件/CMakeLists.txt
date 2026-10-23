cmake_minimum_required(VERSION 3.20)
project(driver_interface_demo C)

# CMSIS 头文件根目录，需包含 CMSIS/Core/Include 与 cmsis_device_f1/Include。
# 交叉编译时用 -DCMSIS_DIR=<路径> 传入；宿主机目标不需要。
set(CMSIS_DIR "" CACHE PATH "CMSIS 与 cmsis_device_f1 所在的根目录")

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")

    if(NOT CMSIS_DIR)
        message(FATAL_ERROR "交叉编译需要 -DCMSIS_DIR=<CMSIS 头文件根目录>")
    endif()

    stm32f103_firmware(blink_fw
        board_stm32f103.c
        app/blink_logic.c
        drivers/gpio.c
        drivers/uart.c
        drivers/tick.c
        offset_check.c
        "${CMSIS_DIR}/cmsis_device_f1/Source/Templates/system_stm32f1xx.c")
    target_include_directories(blink_fw PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMSIS_DIR}/CMSIS/Core/Include"
        "${CMSIS_DIR}/cmsis_device_f1/Include")
else()
    add_executable(blink_host
        board_host.c
        app/blink_logic.c)
    target_compile_options(blink_host PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(blink_host PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()