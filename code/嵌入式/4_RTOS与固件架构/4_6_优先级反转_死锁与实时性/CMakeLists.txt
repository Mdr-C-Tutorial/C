cmake_minimum_required(VERSION 3.20)
project(rtos_realtime_examples C)

# FreeRTOS-Kernel 源码不在仓库里：
#   git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
# 构建时用 -DFREERTOS_KERNEL_PATH=<上面的目录> 指定
include("${CMAKE_CURRENT_LIST_DIR}/../../common/freertos/freertos.cmake")
emb_freertos_add_kernel()

set(EMB_FREERTOS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common/freertos")

# rta 不依赖 FreeRTOS，只在主机上构建（响应时间分析在 PC 上做）
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(rta "${CMAKE_CURRENT_LIST_DIR}/rta.c")
    target_compile_options(rta PRIVATE -std=c17 -Wall -Wextra)
    target_link_libraries(rta PRIVATE m)
endif()

if(CMAKE_CROSSCOMPILING)
    if(NOT CMSIS_INCLUDE_DIR)
        message(FATAL_ERROR "目标构建需要 -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")

    stm32f103_firmware(inversion
        "${CMAKE_CURRENT_LIST_DIR}/inversion.c"
        "${EMB_FREERTOS_DIR}/rtos_console_uart.c")
    target_include_directories(inversion PRIVATE
        "${EMB_FREERTOS_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMSIS_INCLUDE_DIR}")
    target_link_libraries(inversion PRIVATE freertos_kernel freertos_config)
else()
    add_compile_options(-std=c11 -O2 -Wall -Wextra)
    set(EMB_SIM_SRCS
        "${EMB_FREERTOS_DIR}/rtos_console_sim.c"
        "${EMB_FREERTOS_DIR}/idle_stub.c")

    # 二值信号量版与互斥量版共用 inversion.c
    foreach(_app inversion inversion_mutex)
        add_executable(${_app} "${CMAKE_CURRENT_LIST_DIR}/inversion.c" ${EMB_SIM_SRCS})
        target_include_directories(${_app} PRIVATE "${EMB_FREERTOS_DIR}")
        target_link_libraries(${_app} PRIVATE freertos_kernel freertos_config)
    endforeach()
    target_compile_definitions(inversion_mutex PRIVATE USE_MUTEX=1)

    # 死锁的三种变体共用 deadlock.c
    foreach(_app deadlock deadlock_timeout deadlock_fixed)
        add_executable(${_app} "${CMAKE_CURRENT_LIST_DIR}/deadlock.c" ${EMB_SIM_SRCS})
        target_include_directories(${_app} PRIVATE "${EMB_FREERTOS_DIR}")
        target_link_libraries(${_app} PRIVATE freertos_kernel freertos_config)
    endforeach()
    target_compile_definitions(deadlock_timeout PRIVATE WITH_TIMEOUT)
    target_compile_definitions(deadlock_fixed PRIVATE FIXED_ORDER)

    add_executable(deadline_monitor "${CMAKE_CURRENT_LIST_DIR}/deadline_monitor.c" ${EMB_SIM_SRCS})
    target_include_directories(deadline_monitor PRIVATE "${EMB_FREERTOS_DIR}")
    target_link_libraries(deadline_monitor PRIVATE freertos_kernel freertos_config)
endif()