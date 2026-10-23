cmake_minimum_required(VERSION 3.20)
project(firmware_architecture C)

# 6.1 节的示例工程：分层、配置与资源预算。
# 需要 CMSIS 头文件根目录（含 CMSIS/Core/Include 与 cmsis_device_f1/Include）。

set(CMSIS_DIR "" CACHE PATH "CMSIS 与 cmsis_device_f1 所在的根目录")
set(BOARD "bluepill" CACHE STRING "板级配置：bluepill 或 minimal")
set_property(CACHE BOARD PROPERTY STRINGS bluepill minimal)

if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "本工程只构建 STM32F103 固件：cmake -B build --toolchain ../../common/arm-none-eabi.cmake")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")

if(NOT CMSIS_DIR)
    message(FATAL_ERROR "需要 -DCMSIS_DIR=<CMSIS 头文件根目录>")
endif()

# 把板子名字换成配置头文件名，再由 board.h 引入
if(BOARD STREQUAL "bluepill")
    set(BOARD_HEADER "board_bluepill.h")
elseif(BOARD STREQUAL "minimal")
    set(BOARD_HEADER "board_minimal.h")
else()
    message(FATAL_ERROR "未知的 BOARD=${BOARD}，可选 bluepill 或 minimal")
endif()

# size_demo：LED + UART printf，用于观察编译选项对体积的影响
stm32f103_firmware(size_demo
    size_demo/main.c
    size_demo/retarget.c)
target_include_directories(size_demo PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/size_demo"
    "${CMSIS_DIR}/CMSIS/Core/Include"
    "${CMSIS_DIR}/cmsis_device_f1/Include")

# 同一份源文件打开浮点 printf，体积明显变大
stm32f103_firmware(size_demo_float
    size_demo/main.c
    size_demo/retarget.c)
target_compile_definitions(size_demo_float PRIVATE USE_FLOAT_PRINT=1)
target_link_options(size_demo_float PRIVATE -u _printf_float)
target_include_directories(size_demo_float PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/size_demo"
    "${CMSIS_DIR}/CMSIS/Core/Include"
    "${CMSIS_DIR}/cmsis_device_f1/Include")

# 配置检查：按选定的板子编译配置头文件，错误的组合在这里就报错
add_library(config_check OBJECT config/config_check.c)
target_link_libraries(config_check PRIVATE stm32f103_options)
target_compile_definitions(config_check PRIVATE BOARD_HEADER="${BOARD_HEADER}")
target_include_directories(config_check PRIVATE "${CMAKE_CURRENT_LIST_DIR}/config")

# 预算检查：构建后调用脚本读 ELF，超出预算就让构建失败
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(TARGET size_demo POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/budget/check_budget.py"
        "$<TARGET_FILE:size_demo>"
        --budget "${CMAKE_CURRENT_LIST_DIR}/budget/budget.json"
        --size-tool "${CMAKE_SIZE}"
    VERBATIM)

# stack_demo：只为 -fstack-usage 生成 .su 文件，不链接
add_library(stack_demo OBJECT stack/demo.c)
target_compile_options(stack_demo PRIVATE
    -std=c11 -mcpu=cortex-m3 -mthumb -O2 -fstack-usage)