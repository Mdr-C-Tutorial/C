cmake_minimum_required(VERSION 3.20)
project(ch7_2_rtos_recorder C)

# 两个构建目标：sim（Windows/Linux 主机）与 stm32f103（目标板）。
#
#   cmake -B build_sim -G Ninja -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录> \
#         -DLITTLEFS_PATH=<littlefs 目录>
#   cmake --build build_sim && ./build_sim/recorder_sim
#
#   cmake -B build_stm32 -G Ninja --toolchain ../../common/arm-none-eabi.cmake \
#         -DARM_TOOLCHAIN_DIR=<工具链 bin> -DCMSIS_INCLUDE_DIR=<CMSIS 头文件> \
#         -DFREERTOS_KERNEL_PATH=<...> -DLITTLEFS_PATH=<...>
#
# FreeRTOS-Kernel V11.3.0 与 littlefs v2.11.3 的源码不进仓库，按 README 拉取。

set(FREERTOS_KERNEL_PATH "" CACHE PATH "FreeRTOS-Kernel 源码目录")
set(LITTLEFS_PATH "" CACHE PATH "littlefs 源码目录")

# 两个构建目标用不同的 FreeRTOSConfig.h：中断优先级与堆大小只对目标板有意义。
# 配置目录各自只放一份，构建时加到 include 路径最前面。
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/target_config"
     "${CMAKE_CURRENT_BINARY_DIR}/sim_config")
configure_file("${CMAKE_CURRENT_LIST_DIR}/FreeRTOSConfig_target.h"
               "${CMAKE_CURRENT_BINARY_DIR}/target_config/FreeRTOSConfig.h" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/FreeRTOSConfig_sim.h"
               "${CMAKE_CURRENT_BINARY_DIR}/sim_config/FreeRTOSConfig.h" COPYONLY)

set(APP_SRCS
    app/tasks.c
    app/log.c
    app/console_task.c)

set(FREERTOS_CORE_SRCS
    "${FREERTOS_KERNEL_PATH}/tasks.c"
    "${FREERTOS_KERNEL_PATH}/queue.c"
    "${FREERTOS_KERNEL_PATH}/list.c"
    "${FREERTOS_KERNEL_PATH}/timers.c"
    "${FREERTOS_KERNEL_PATH}/event_groups.c"
    "${FREERTOS_KERNEL_PATH}/stream_buffer.c"
    "${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c")

set(LITTLEFS_SRCS
    "${LITTLEFS_PATH}/lfs.c"
    "${LITTLEFS_PATH}/lfs_util.c")

set(COMMON_INCLUDES
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/app"
    "${LITTLEFS_PATH}"
    "${LITTLEFS_PATH}/bd")

# 公共的 FreeRTOS 封装：挑选移植层，并把 freeertos_config 与支持文件复制到构建目录。
# 本页用工程根目录的 FreeRTOSConfig.h 覆盖公共模板。
include("${CMAKE_CURRENT_LIST_DIR}/../../common/freertos/freertos.cmake")
set(EMB_FREERTOS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common/freertos")

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    stm32f103_firmware(recorder_stm32
        ${APP_SRCS}
        ${FREERTOS_CORE_SRCS}
        ${LITTLEFS_SRCS}
        "${CMAKE_CURRENT_LIST_DIR}/stm32_board.c"
        "${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM3/port.c")
    target_include_directories(recorder_stm32 PRIVATE
        ${COMMON_INCLUDES}
        "${CMSIS_INCLUDE_DIR}"
        "${FREERTOS_KERNEL_PATH}/include"
        "${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM3"
        "${CMAKE_CURRENT_BINARY_DIR}/target_config")
    target_compile_definitions(recorder_stm32 PRIVATE LFS_THREADSAFE=1)
    target_link_libraries(recorder_stm32 PRIVATE m)
else()
    if(WIN32)
        set(PORT_SRC "${FREERTOS_KERNEL_PATH}/portable/MSVC-MingW/port.c")
        set(PORT_LIBS winmm)
    else()
        set(PORT_SRC "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix/port.c")
        set(PORT_LIBS pthread)
    endif()

    add_executable(recorder_sim
        ${APP_SRCS}
        ${FREERTOS_CORE_SRCS}
        ${LITTLEFS_SRCS}
        "${LITTLEFS_PATH}/bd/lfs_rambd.c"
        "${CMAKE_CURRENT_LIST_DIR}/sim_main.c"
        "${PORT_SRC}")
    target_include_directories(recorder_sim PRIVATE
        ${COMMON_INCLUDES}
        "${FREERTOS_KERNEL_PATH}/include"
        "${FREERTOS_KERNEL_PATH}/portable/MSVC-MingW"
        "${CMAKE_CURRENT_BINARY_DIR}/sim_config")
    # 模拟器上关掉 littlefs 的调试打印；目标板上保留 LFS_ERROR，便于定位
    target_compile_definitions(recorder_sim PRIVATE
        LFS_THREADSAFE=1 LFS_NO_DEBUG=1 LFS_NO_WARN=1 LFS_NO_ERROR=1)
    target_compile_options(recorder_sim PRIVATE -std=c11 -O2 -Wall -Wextra)
    target_link_libraries(recorder_sim PRIVATE ${PORT_LIBS} m)

    # 主机测试：只测 littlefs 上的 CSV 记录格式与重新挂载，不链接 FreeRTOS
    add_executable(test_log
        tests/test_log.c
        ${LITTLEFS_SRCS}
        "${LITTLEFS_PATH}/bd/lfs_rambd.c"
        "${CMAKE_CURRENT_LIST_DIR}/tests/sim_lfs.c")
    target_include_directories(test_log PRIVATE ${COMMON_INCLUDES})
    target_compile_definitions(test_log PRIVATE
        LFS_THREADSAFE=1 LFS_NO_DEBUG=1 LFS_NO_WARN=1 LFS_NO_ERROR=1)
    target_compile_options(test_log PRIVATE -std=c17 -O2 -Wall -Wextra)
    target_link_libraries(test_log PRIVATE m)
endif()