cmake_minimum_required(VERSION 3.20)
project(rtos_architecture_demo C)

# FreeRTOS-Kernel 源码不在仓库里：
#   git clone --depth 1 --branch V11.3.0 https://github.com/FreeRTOS/FreeRTOS-Kernel.git
# 构建时用 -DFREERTOS_KERNEL_PATH=<上面的目录> 指定
include("${CMAKE_CURRENT_LIST_DIR}/freertos.cmake")

set(EMB_FREERTOS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common/freertos")
set(EMB_THIS_DIR "${CMAKE_CURRENT_LIST_DIR}")

add_compile_options(-std=c11 -O2 -Wall -Wextra)

if(CMAKE_CROSSCOMPILING)
    # 目标构建：只做编译检查，DWT 时间戳与看门狗都在真实硬件上才有意义
    if(NOT CMSIS_INCLUDE_DIR)
        message(FATAL_ERROR "目标构建需要 -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    add_library(emb_target_common OBJECT
        "${EMB_THIS_DIR}/common/target_hooks.c"
        "${EMB_THIS_DIR}/target/trace_dwt.c")
    target_include_directories(emb_target_common PRIVATE
        "${EMB_THIS_DIR}" "${CMSIS_INCLUDE_DIR}")
    target_link_libraries(emb_target_common PRIVATE freertos_kernel freertos_config)
else()
    add_library(emb_sim_common OBJECT
        "${EMB_THIS_DIR}/common/sim_hooks.c"
        "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_include_directories(emb_sim_common PUBLIC "${EMB_THIS_DIR}" "${EMB_FREERTOS_DIR}")
    target_link_libraries(emb_sim_common PUBLIC freertos_kernel freertos_config)

    # 活动对象：状态机 + 事件队列 + 每个对象一个任务
    add_executable(ao_demo
        "${EMB_THIS_DIR}/ao/main_ao.c"
        "${EMB_THIS_DIR}/ao/ao.c"
        "${EMB_THIS_DIR}/ao/led_fsm.c"
        "${EMB_THIS_DIR}/ao/led_ao.c")
    target_link_libraries(ao_demo PRIVATE emb_sim_common)

    # 监视任务 + 用打印代替的看门狗
    add_executable(supervisor_demo "${EMB_THIS_DIR}/supervisor/supervisor.c")
    target_link_libraries(supervisor_demo PRIVATE emb_sim_common)

    # 跟踪缓冲与运行时统计
    # trace_buf 自带跟踪钩子，所以这一份 sim_hooks.c 用 TRACE_OWN_HOOK 关掉空实现
    add_executable(trace_demo
        "${EMB_THIS_DIR}/trace/trace_buf.c"
        "${EMB_THIS_DIR}/common/sim_hooks.c"
        "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_compile_definitions(trace_demo PRIVATE TRACE_OWN_HOOK=1)
    target_link_libraries(trace_demo PRIVATE freertos_kernel freertos_config)
    target_include_directories(trace_demo PRIVATE
        "${EMB_THIS_DIR}" "${EMB_FREERTOS_DIR}")

    add_executable(runtime_stats_demo "${EMB_THIS_DIR}/trace/runtime_stats_sim.c")
    target_link_libraries(runtime_stats_demo PRIVATE emb_sim_common)

    # 主机单元测试：只编状态机，不碰 RTOS
    add_executable(test_led_fsm
        "${EMB_THIS_DIR}/unit_test/test_led_fsm.c"
        "${EMB_THIS_DIR}/ao/led_fsm.c")
    target_include_directories(test_led_fsm PRIVATE "${EMB_THIS_DIR}/ao")
endif()
