cmake_minimum_required(VERSION 3.20)
project(freertos_config_demo C)

# FreeRTOS-Kernel 源码不在仓库里：
#   git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
# 构建时用 -DFREERTOS_KERNEL_PATH=<上面的目录> 指定。
# 目标构建还需要 -DCMSIS_INCLUDE_DIR=<CMSIS Core 与 cmsis_device_f1 的头文件目录>。
include("${CMAKE_CURRENT_LIST_DIR}/freertos.cmake")

set(EMB_FREERTOS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common/freertos")

add_compile_options(-std=c11 -O2 -Wall -Wextra)

if(CMAKE_CROSSCOMPILING)
    # STM32F103C8：两个任务分别闪 PC13 与打印节拍数
    if(NOT CMSIS_INCLUDE_DIR)
        message(FATAL_ERROR "目标构建需要 -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    stm32f103_firmware(stm32_freertos
        "${CMAKE_CURRENT_LIST_DIR}/main_stm32.c"
        "${CMAKE_CURRENT_LIST_DIR}/target_hooks.c"
        "${EMB_FREERTOS_DIR}/rtos_console_uart.c")
    target_include_directories(stm32_freertos PRIVATE
        "${EMB_FREERTOS_DIR}" "${CMSIS_INCLUDE_DIR}")
    target_link_libraries(stm32_freertos PRIVATE freertos_kernel freertos_config)

    # tickless_stop.c 覆盖移植层的弱函数，单独编成目标文件做编译检查
    add_library(tickless_stop OBJECT "${CMAKE_CURRENT_LIST_DIR}/tickless_stop.c")
    target_include_directories(tickless_stop PRIVATE
        "${EMB_FREERTOS_DIR}" "${CMSIS_INCLUDE_DIR}")
    target_link_libraries(tickless_stop PRIVATE freertos_kernel freertos_config)
    target_compile_definitions(tickless_stop PRIVATE configUSE_TICKLESS_IDLE=2)
else()
    # 模拟器示例：控制台走 rtos_console_sim.c，钩子由 hooks_stub.c 或 hooks_sim.c 提供
    foreach(_app sw_timer_sim task_list_sim)
        add_executable(${_app}
            "${CMAKE_CURRENT_LIST_DIR}/${_app}.c"
            "${CMAKE_CURRENT_LIST_DIR}/hooks_stub.c"
            "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
        target_include_directories(${_app} PRIVATE "${EMB_FREERTOS_DIR}")
        target_link_libraries(${_app} PRIVATE freertos_kernel freertos_config)
    endforeach()

    # hooks_sim 自己定义全部钩子，不链接 hooks_stub.c
    add_executable(hooks_sim
        "${CMAKE_CURRENT_LIST_DIR}/hooks_sim.c"
        "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_include_directories(hooks_sim PRIVATE "${EMB_FREERTOS_DIR}")
    target_link_libraries(hooks_sim PRIVATE freertos_kernel freertos_config)
endif()
