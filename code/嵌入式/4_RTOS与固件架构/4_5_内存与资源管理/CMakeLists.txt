cmake_minimum_required(VERSION 3.20)
project(rtos_memory_examples C)

# 本页示例需要打开静态分配与栈溢出检测，公共的
# common/freertos/FreeRTOSConfig_sim.h 没有这两项，
# 所以这里用本目录的 FreeRTOSConfig_mem.h 覆盖配置模板。
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel" CACHE PATH "FreeRTOS-Kernel 源码路径")
if(NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR "请用 -DFREERTOS_KERNEL_PATH=<路径> 指定 FreeRTOS-Kernel 源码目录")
endif()

# 公共的 FreeRTOS 构建封装负责选移植层；先把配置替换成本页的版本
include("${CMAKE_CURRENT_LIST_DIR}/../../common/freertos/freertos.cmake")
set(EMB_FREERTOS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common/freertos")

set(FREERTOS_CORE_SRCS
    "${FREERTOS_KERNEL_PATH}/tasks.c"
    "${FREERTOS_KERNEL_PATH}/queue.c"
    "${FREERTOS_KERNEL_PATH}/list.c"
    "${FREERTOS_KERNEL_PATH}/timers.c"
    "${FREERTOS_KERNEL_PATH}/event_groups.c"
    "${FREERTOS_KERNEL_PATH}/stream_buffer.c"
    "${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c"
)

if(CMAKE_CROSSCOMPILING)
    if(NOT CMSIS_INCLUDE_DIR)
        message(FATAL_ERROR "目标构建需要 -DCMSIS_INCLUDE_DIR=<CMSIS 头文件目录>")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    set(PORT_SRC "${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM3/port.c")
    set(PORT_INC "${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM3")
else()
    if(WIN32)
        set(PORT_SRC "${FREERTOS_KERNEL_PATH}/portable/MSVC-MingW/port.c")
        set(PORT_INC "${FREERTOS_KERNEL_PATH}/portable/MSVC-MingW")
        set(PORT_LIBS winmm)
    else()
        set(PORT_SRC
            "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix/port.c"
            "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c")
        set(PORT_INC
            "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix"
            "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix/utils")
        set(PORT_LIBS pthread)
    endif()
endif()

# 本页专用的配置模板：复制到构建目录并改名为 FreeRTOSConfig.h
set(_cfg_dir "${CMAKE_BINARY_DIR}/mem_config")
file(MAKE_DIRECTORY "${_cfg_dir}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/FreeRTOSConfig_mem.h"
               "${_cfg_dir}/FreeRTOSConfig.h" COPYONLY)
add_library(mem_freertos_config INTERFACE)
target_include_directories(mem_freertos_config SYSTEM INTERFACE "${_cfg_dir}")

# mem_pool 与主机单元测试不依赖 FreeRTOS；单元测试只在主机上构建
if(NOT CMAKE_CROSSCOMPILING)
    add_library(mdrmempool STATIC "${CMAKE_CURRENT_LIST_DIR}/mem_pool.c")
    target_compile_options(mdrmempool PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(mdrmempool PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

    # 调试版检查（重复归还）编进 mem_pool.c 本身，所以单元测试直接编译源码，
    # 不链接上面那份不带 MEM_POOL_DEBUG 的库
    add_executable(test_mem_pool
        "${CMAKE_CURRENT_LIST_DIR}/test_mem_pool.c"
        "${CMAKE_CURRENT_LIST_DIR}/mem_pool.c")
    target_compile_definitions(test_mem_pool PRIVATE MEM_POOL_DEBUG)
    target_compile_options(test_mem_pool PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(test_mem_pool PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()

function(mem_add_app target)
    add_executable(${target} ${ARGN} ${FREERTOS_CORE_SRCS} ${PORT_SRC})
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        "${FREERTOS_KERNEL_PATH}/include"
        ${PORT_INC})
    target_link_libraries(${target} PRIVATE mem_freertos_config)
    if(PORT_LIBS)
        target_link_libraries(${target} PRIVATE ${PORT_LIBS})
    endif()
    if(CMAKE_CROSSCOMPILING)
        target_compile_options(${target} PRIVATE -mcpu=cortex-m3 -mthumb -std=c11)
        target_link_options(${target} PRIVATE -mcpu=cortex-m3 -mthumb
            -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections)
        target_link_libraries(${target} PRIVATE stm32f103_options stm32f103_startup)
        set_target_properties(${target} PROPERTIES SUFFIX ".elf")
        target_link_options(${target} PRIVATE
            "-T${STM32F103_COMMON_BIN_DIR}/stm32f103c8.ld"
            "-Wl,-Map=$<TARGET_FILE_DIR:${target}>/${target}.map")
    endif()
endfunction()

if(CMAKE_CROSSCOMPILING)
    foreach(_app ram_map static_tasks main_stack_bug)
        mem_add_app(${_app} "${CMAKE_CURRENT_LIST_DIR}/${_app}.c")
        target_include_directories(${_app} PRIVATE "${CMSIS_INCLUDE_DIR}")
    endforeach()

    mem_add_app(static_tasks_dynamic "${CMAKE_CURRENT_LIST_DIR}/static_tasks.c")
    target_compile_definitions(static_tasks_dynamic PRIVATE USE_DYNAMIC_ALLOC=1)
    target_include_directories(static_tasks_dynamic PRIVATE "${CMSIS_INCLUDE_DIR}")

    mem_add_app(main_stack_bug_local "${CMAKE_CURRENT_LIST_DIR}/main_stack_bug.c")
    target_compile_definitions(main_stack_bug_local PRIVATE SHOW_BUG)
    target_include_directories(main_stack_bug_local PRIVATE "${CMSIS_INCLUDE_DIR}")

    # stack_usage 只需要编译，配合 -fstack-usage 生成 .su 文件
    add_library(stack_usage OBJECT "${CMAKE_CURRENT_LIST_DIR}/stack_usage.c")
    target_compile_options(stack_usage PRIVATE -O2 -mcpu=cortex-m3 -mthumb -std=c11 -fstack-usage)
    target_include_directories(stack_usage PRIVATE "${CMSIS_INCLUDE_DIR}")
elseif(WIN32)
    add_compile_options(-std=c11 -O2 -Wall -Wextra)
    mem_add_app(heap_frag_sim "${CMAKE_CURRENT_LIST_DIR}/heap_frag_sim.c"
                "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_include_directories(heap_frag_sim PRIVATE "${EMB_FREERTOS_DIR}")
    mem_add_app(static_tasks "${CMAKE_CURRENT_LIST_DIR}/static_tasks.c"
                "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_include_directories(static_tasks PRIVATE "${EMB_FREERTOS_DIR}")
    mem_add_app(pool_sim "${CMAKE_CURRENT_LIST_DIR}/pool_sim.c"
                "${CMAKE_CURRENT_LIST_DIR}/mem_pool.c"
                "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_include_directories(pool_sim PRIVATE "${EMB_FREERTOS_DIR}")
else()
    add_compile_options(-std=c11 -O2 -Wall -Wextra)
    mem_add_app(heap_frag_sim "${CMAKE_CURRENT_LIST_DIR}/heap_frag_sim.c"
                "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_include_directories(heap_frag_sim PRIVATE "${EMB_FREERTOS_DIR}")
    mem_add_app(static_tasks "${CMAKE_CURRENT_LIST_DIR}/static_tasks.c"
                "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_include_directories(static_tasks PRIVATE "${EMB_FREERTOS_DIR}")
    mem_add_app(pool_sim "${CMAKE_CURRENT_LIST_DIR}/pool_sim.c"
                "${CMAKE_CURRENT_LIST_DIR}/mem_pool.c"
                "${EMB_FREERTOS_DIR}/rtos_console_sim.c")
    target_include_directories(pool_sim PRIVATE "${EMB_FREERTOS_DIR}")
endif()