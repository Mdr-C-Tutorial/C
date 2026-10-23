# freertos.cmake：把本节的示例接到 FreeRTOS-Kernel 上。
#
# 与 common/freertos/freertos.cmake 的区别只有配置来源：这里用本节目录下的
# 一份 FreeRTOSConfig.h，模拟器与目标共用，靠 __ARM_ARCH 区分芯片相关项。

include_guard(GLOBAL)

set(FREERTOS_KERNEL_PATH "" CACHE PATH "FreeRTOS-Kernel 源码目录")
if(NOT FREERTOS_KERNEL_PATH)
    message(FATAL_ERROR "请用 -DFREERTOS_KERNEL_PATH=<路径> 指定 FreeRTOS-Kernel 源码目录")
endif()

set(_cfg_dir "${CMAKE_BINARY_DIR}/freertos_config")
file(MAKE_DIRECTORY "${_cfg_dir}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/FreeRTOSConfig.h"
               "${_cfg_dir}/FreeRTOSConfig.h" COPYONLY)

add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE "${_cfg_dir}")

if(CMAKE_CROSSCOMPILING)
    set(FREERTOS_PORT "GCC_ARM_CM3" CACHE STRING "FreeRTOS 移植层" FORCE)
elseif(WIN32)
    set(FREERTOS_PORT "MSVC_MINGW" CACHE STRING "FreeRTOS 移植层" FORCE)
else()
    set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "FreeRTOS 移植层" FORCE)
endif()
set(FREERTOS_HEAP "4" CACHE STRING "FreeRTOS 堆实现" FORCE)

if(CMAKE_CROSSCOMPILING)
    # 内核的 CMake 工程不带目标架构选项，在 add_subdirectory 之前加到目录级，
    # 让内核源码也按 Cortex-M3 编译
    add_compile_options(-mcpu=cortex-m3 -mthumb -std=c11)
endif()

add_subdirectory("${FREERTOS_KERNEL_PATH}" "${CMAKE_BINARY_DIR}/freertos_kernel")

if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    target_link_libraries(freertos_kernel INTERFACE winmm)
elseif(NOT WIN32 AND NOT CMAKE_CROSSCOMPILING)
    target_link_libraries(freertos_kernel INTERFACE pthread)
endif()
