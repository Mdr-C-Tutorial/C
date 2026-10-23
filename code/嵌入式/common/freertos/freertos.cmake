# freertos.cmake：把 FreeRTOS-Kernel 接进本教程的 CMake 工程。
#
# 用法（工程目录下的 CMakeLists.txt）：
#   set(FREERTOS_KERNEL_PATH "<FreeRTOS-Kernel 源码目录>" CACHE PATH "")
#   include(".../common/freertos/freertos.cmake")
#   emb_freertos_add_kernel()
#   ...
#   target_link_libraries(<目标> PRIVATE freertos_kernel)
#
# 它按构建目标自动选择移植层与配置模板：
#   - 交叉编译（arm-none-eabi，Generic 系统）：GCC_ARM_CM3 + FreeRTOSConfig_stm32.h
#   - Windows 主机：MSVC_MINGW + FreeRTOSConfig_sim.h
#   - 其他主机（Linux、macOS）：GCC_POSIX + FreeRTOSConfig_sim.h
# 配置模板被复制到构建目录的 freertos_config/ 并改名为 FreeRTOSConfig.h，
# 因为内核的 CMake 只认这个文件名。控制台的两种实现也复制到构建目录的
# freertos_support/：Windows 上 Ninja 与 mingw32-make 遇到源码路径里的中文
# 会生成无法创建的中间目录，复制之后编译命令里只剩构建目录的路径。
#
# 讲解见教程 4.2 节「任务、调度与时间」。

include_guard(GLOBAL)

# 函数体里的 CMAKE_CURRENT_LIST_DIR 指向调用方的 CMakeLists，先在这里记下本目录
set(EMB_FREERTOS_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(FREERTOS_KERNEL_PATH "" CACHE PATH "FreeRTOS-Kernel 源码目录")
if(NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR "请用 -DFREERTOS_KERNEL_PATH=<路径> 指定 FreeRTOS-Kernel 源码目录")
endif()

function(emb_freertos_add_kernel)
    if(CMAKE_CROSSCOMPILING)
        set(_port "GCC_ARM_CM3")
        set(_cfg "${EMB_FREERTOS_DIR}/FreeRTOSConfig_stm32.h")
    elseif(WIN32)
        set(_port "MSVC_MINGW")
        set(_cfg "${EMB_FREERTOS_DIR}/FreeRTOSConfig_sim.h")
    else()
        set(_port "GCC_POSIX")
        set(_cfg "${EMB_FREERTOS_DIR}/FreeRTOSConfig_sim.h")
    endif()

    set(_cfg_dir "${CMAKE_BINARY_DIR}/freertos_config")
    file(MAKE_DIRECTORY "${_cfg_dir}")
    configure_file("${_cfg}" "${_cfg_dir}/FreeRTOSConfig.h" COPYONLY)

    # 控制台实现与共用的头文件复制到构建目录，见文件开头的说明
    set(_support_dir "${CMAKE_BINARY_DIR}/freertos_support")
    file(MAKE_DIRECTORY "${_support_dir}")
    foreach(_f rtos_console.h rtos_console_sim.c rtos_console_uart.c idle_stub.c)
        configure_file("${EMB_FREERTOS_DIR}/${_f}" "${_support_dir}/${_f}" COPYONLY)
    endforeach()
    set(EMB_FREERTOS_SUPPORT_DIR "${_support_dir}" CACHE INTERNAL "第 4 章共用的控制台实现所在目录")

    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE "${_cfg_dir}")

    if(CMAKE_CROSSCOMPILING)
        # 内核自己的 CMake 工程不带目标架构选项，这里在 add_subdirectory 之前
        # 加到目录级，让内核源码也按 Cortex-M3 编译
        add_compile_options(-mcpu=cortex-m3 -mthumb -std=c11)
    endif()

    set(FREERTOS_PORT "${_port}" CACHE STRING "FreeRTOS 移植层" FORCE)
    set(FREERTOS_HEAP "4" CACHE STRING "FreeRTOS 堆实现" FORCE)
    add_subdirectory("${FREERTOS_KERNEL_PATH}" "${CMAKE_BINARY_DIR}/freertos_kernel")

    if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
        target_link_libraries(freertos_kernel INTERFACE winmm)
    elseif(NOT WIN32 AND NOT CMAKE_CROSSCOMPILING)
        target_link_libraries(freertos_kernel INTERFACE pthread)
    endif()
endfunction()
