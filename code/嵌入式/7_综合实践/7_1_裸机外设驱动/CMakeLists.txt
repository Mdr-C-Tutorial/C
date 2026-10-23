cmake_minimum_required(VERSION 3.20)
project(ch7_1_baremetal_drivers C)

# 目标构建：Blue Pill（STM32F103C8）
#   cmake -B build -G Ninja --toolchain ../../common/arm-none-eabi.cmake \
#         -DARM_TOOLCHAIN_DIR=<arm-none-eabi-gcc 所在目录> \
#         -DCMSIS_INCLUDE_DIR=<CMSIS Core 与 cmsis_device_f1 头文件目录>
# 主机测试：同一份驱动源码，换一个假总线
#   cmake -B build_host -G Ninja && cmake --build build_host

include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")

if(CMAKE_CROSSCOMPILING)
    if(NOT CMSIS_INCLUDE_DIR)
        message(FATAL_ERROR "目标构建需要 -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>")
    endif()
    stm32f103_firmware(mdr_bare
        app/main.c
        app/cli.c
        board/board_stm32f103.c
        drivers/i2c_soft.c
        drivers/mpu6050.c)
    target_include_directories(mdr_bare PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/board"
        "${CMAKE_CURRENT_LIST_DIR}/drivers"
        "${CMAKE_CURRENT_LIST_DIR}/app"
        "${CMSIS_INCLUDE_DIR}")
    target_link_libraries(mdr_bare PRIVATE m)
else()
    add_executable(test_mpu6050
        tests/test_mpu6050.c
        drivers/mpu6050.c)
    target_compile_options(test_mpu6050 PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(test_mpu6050 PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/drivers")
endif()