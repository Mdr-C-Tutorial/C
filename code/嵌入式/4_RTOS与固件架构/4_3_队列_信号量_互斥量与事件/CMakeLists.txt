cmake_minimum_required(VERSION 3.20)
project(rtos_ipc_examples C)

# FreeRTOS-Kernel 源码不在仓库里：
#   git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
# 构建时用 -DFREERTOS_KERNEL_PATH=<上面的目录> 指定
include("${CMAKE_CURRENT_LIST_DIR}/../../common/freertos/freertos.cmake")
emb_freertos_add_kernel()

# 控制台实现与头文件由 freertos.cmake 复制到构建目录，避开中文路径带来的问题
set(EMB_SUPPORT_DIR "${EMB_FREERTOS_SUPPORT_DIR}")

set(APPS producer_consumer mailbox button_semaphore console_mutex
         console_gatekeeper startup_events notify_counter)

if(CMAKE_CROSSCOMPILING)
    if(NOT CMSIS_INCLUDE_DIR)
        message(FATAL_ERROR "目标构建需要 -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    foreach(_app IN LISTS APPS)
        stm32f103_firmware(${_app}
            "${CMAKE_CURRENT_LIST_DIR}/${_app}.c"
            "${EMB_SUPPORT_DIR}/rtos_console_uart.c")
        target_include_directories(${_app} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}"
            "${EMB_SUPPORT_DIR}"
            "${CMSIS_INCLUDE_DIR}")
        target_link_libraries(${_app} PRIVATE freertos_kernel freertos_config)
    endforeach()
else()
    add_compile_options(-std=c11 -O2 -Wall -Wextra)
    foreach(_app IN LISTS APPS)
        add_executable(${_app}
            "${CMAKE_CURRENT_LIST_DIR}/${_app}.c"
            "${EMB_SUPPORT_DIR}/rtos_console_sim.c"
            "${EMB_SUPPORT_DIR}/idle_stub.c")
        target_include_directories(${_app} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}"
            "${EMB_SUPPORT_DIR}")
        target_link_libraries(${_app} PRIVATE freertos_kernel freertos_config)
    endforeach()
endif()
