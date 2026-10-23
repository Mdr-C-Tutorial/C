cmake_minimum_required(VERSION 3.20)
project(rtos_tasks_demo C)

# FreeRTOS-Kernel 源码不在仓库里：
#   git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
# 构建时用 -DFREERTOS_KERNEL_PATH=<上面的目录> 指定
include("${CMAKE_CURRENT_LIST_DIR}/../../common/freertos/freertos.cmake")
emb_freertos_add_kernel()

# 控制台实现与头文件由 freertos.cmake 复制到构建目录，避开中文路径带来的问题
set(EMB_SUPPORT_DIR "${EMB_FREERTOS_SUPPORT_DIR}")

if(CMAKE_CROSSCOMPILING)
    # STM32F103C8 目标：两个任务分别闪 PC13 的 LED 与打印。
    # CMSIS_5 5.9.0 的 CMSIS/Core/Include 与 cmsis_device_f1 v4.3.5 的 Include
    # 不在仓库里，用 -DCMSIS_INCLUDE_DIR=<目录> 指定（两个目录的内容放在一起即可）。
    if(NOT CMSIS_INCLUDE_DIR)
        message(FATAL_ERROR "目标构建需要 -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    stm32f103_firmware(blinky_rtos
        "${CMAKE_CURRENT_LIST_DIR}/blinky_rtos/main.c"
        "${CMAKE_CURRENT_LIST_DIR}/blinky_rtos/board.c"
        "${EMB_SUPPORT_DIR}/rtos_console_uart.c")
    target_include_directories(blinky_rtos PRIVATE
        "${EMB_SUPPORT_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/blinky_rtos"
        "${CMSIS_INCLUDE_DIR}")
    target_link_libraries(blinky_rtos PRIVATE freertos_kernel freertos_config)
else()
    add_compile_options(-std=c11 -O2 -Wall -Wextra)

    # 模拟器示例共用的源文件；idle_stub.c 给没有定义空闲钩子的示例兜底
    set(EMB_SIM_SRCS
        "${EMB_SUPPORT_DIR}/rtos_console_sim.c"
        "${EMB_SUPPORT_DIR}/idle_stub.c")

    foreach(_app two_tasks task_param delay_drift)
        add_executable(${_app} "${CMAKE_CURRENT_LIST_DIR}/${_app}.c" ${EMB_SIM_SRCS})
        target_include_directories(${_app} PRIVATE "${EMB_SUPPORT_DIR}")
        target_link_libraries(${_app} PRIVATE freertos_kernel freertos_config)
    endforeach()

    # 忙等与阻塞两个版本，同一份源码用 BUSY_WAIT 切换
    foreach(_app starvation_busy starvation_block)
        add_executable(${_app} "${CMAKE_CURRENT_LIST_DIR}/starvation.c" ${EMB_SIM_SRCS})
        target_include_directories(${_app} PRIVATE "${EMB_SUPPORT_DIR}")
        target_link_libraries(${_app} PRIVATE freertos_kernel freertos_config)
    endforeach()
    target_compile_definitions(starvation_busy PRIVATE BUSY_WAIT=1)
    target_compile_definitions(starvation_block PRIVATE BUSY_WAIT=0)

    # 空闲钩子示例自己实现 vApplicationIdleHook()，不链接 idle_stub.c
    add_executable(idle_hook
        "${CMAKE_CURRENT_LIST_DIR}/idle_hook.c"
        "${EMB_SUPPORT_DIR}/rtos_console_sim.c")
    target_include_directories(idle_hook PRIVATE "${EMB_SUPPORT_DIR}")
    target_link_libraries(idle_hook PRIVATE freertos_kernel freertos_config)
endif()
