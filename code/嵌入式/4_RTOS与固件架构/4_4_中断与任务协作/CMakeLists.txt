cmake_minimum_required(VERSION 3.20)
project(rtos_isr_examples C)

# FreeRTOS-Kernel 源码不在仓库里：
#   git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
# 构建时用 -DFREERTOS_KERNEL_PATH=<上面的目录> 指定
include("${CMAKE_CURRENT_LIST_DIR}/../../common/freertos/freertos.cmake")
emb_freertos_add_kernel()

set(EMB_FREERTOS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common/freertos")

if(CMAKE_CROSSCOMPILING)
    # STM32F103C8 目标。CMSIS_5 5.9.0 的 CMSIS/Core/Include 与 cmsis_device_f1
    # v4.3.5 的 Include 不在仓库里，用 -DCMSIS_INCLUDE_DIR=<目录> 指定。
    if(NOT CMSIS_INCLUDE_DIR)
        message(FATAL_ERROR "目标构建需要 -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")

    foreach(_app uart_rx_rtos uart_tx_blocking adc_dma_notify isr_priority)
        stm32f103_firmware(${_app} "${CMAKE_CURRENT_LIST_DIR}/${_app}.c")
        target_include_directories(${_app} PRIVATE
            "${EMB_FREERTOS_DIR}"
            "${CMAKE_CURRENT_LIST_DIR}"
            "${CMSIS_INCLUDE_DIR}")
        target_link_libraries(${_app} PRIVATE freertos_kernel freertos_config)
    endforeach()

    # 反例版本：中断优先级没设，只用于编译与阅读对照
    stm32f103_firmware(isr_priority_wrong "${CMAKE_CURRENT_LIST_DIR}/isr_priority.c")
    target_compile_definitions(isr_priority_wrong PRIVATE WRONG_INIT)
    target_include_directories(isr_priority_wrong PRIVATE
        "${EMB_FREERTOS_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMSIS_INCLUDE_DIR}")
    target_link_libraries(isr_priority_wrong PRIVATE freertos_kernel freertos_config)
else()
    add_compile_options(-std=c11 -O2 -Wall -Wextra)

    # 公共的模拟器控制台实现
    set(EMB_SIM_SRCS
        "${EMB_FREERTOS_DIR}/rtos_console_sim.c"
        "${EMB_FREERTOS_DIR}/idle_stub.c")

    add_executable(isr_notify_sim "${CMAKE_CURRENT_LIST_DIR}/isr_notify_sim.c" ${EMB_SIM_SRCS})
    target_include_directories(isr_notify_sim PRIVATE "${EMB_FREERTOS_DIR}")
    target_link_libraries(isr_notify_sim PRIVATE freertos_kernel freertos_config)

    # 同样的程序，丢掉 higher_woken 的反例
    add_executable(isr_notify_noyield "${CMAKE_CURRENT_LIST_DIR}/isr_notify_sim.c" ${EMB_SIM_SRCS})
    target_compile_definitions(isr_notify_noyield PRIVATE NO_YIELD_FROM_ISR)
    target_include_directories(isr_notify_noyield PRIVATE "${EMB_FREERTOS_DIR}")
    target_link_libraries(isr_notify_noyield PRIVATE freertos_kernel freertos_config)

    # uart_cli_sim 用一个 Windows 线程模拟串口外设，只在 Windows 上构建
    if(WIN32)
        add_executable(uart_cli_sim "${CMAKE_CURRENT_LIST_DIR}/uart_cli_sim.c" ${EMB_SIM_SRCS})
        target_include_directories(uart_cli_sim PRIVATE "${EMB_FREERTOS_DIR}")
        target_link_libraries(uart_cli_sim PRIVATE freertos_kernel freertos_config)
    endif()
endif()