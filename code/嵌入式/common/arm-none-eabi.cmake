# arm-none-eabi.cmake：用 Arm GNU Toolchain 交叉编译 Cortex-M 固件的 CMake 工具链文件。
#
# 用法（在工程目录下）：
#   cmake -B build -G Ninja --toolchain <本文件路径>
#   cmake --build build
# arm-none-eabi-gcc 不在 PATH 中时，加 -DARM_TOOLCHAIN_DIR=<工具链的 bin 目录>。
# 讲解见教程 1.8 节「复位、启动与链接」。

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(ARM_TOOLCHAIN_DIR "" CACHE PATH "arm-none-eabi-gcc 所在的 bin 目录，在 PATH 中时留空")

# 工具链文件在 try_compile 的子工程里会再读一次，把这个设置也传过去
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_TOOLCHAIN_DIR)

if(CMAKE_HOST_WIN32)
    set(_arm_suffix ".exe")
else()
    set(_arm_suffix "")
endif()

if(ARM_TOOLCHAIN_DIR)
    set(_arm_prefix "${ARM_TOOLCHAIN_DIR}/arm-none-eabi-")
else()
    set(_arm_prefix "arm-none-eabi-")
endif()

set(CMAKE_C_COMPILER "${_arm_prefix}gcc${_arm_suffix}")
set(CMAKE_CXX_COMPILER "${_arm_prefix}g++${_arm_suffix}")
set(CMAKE_ASM_COMPILER "${_arm_prefix}gcc${_arm_suffix}")
set(CMAKE_OBJCOPY "${_arm_prefix}objcopy${_arm_suffix}" CACHE FILEPATH "objcopy")
set(CMAKE_SIZE "${_arm_prefix}size${_arm_suffix}" CACHE FILEPATH "size")

# 交叉编译出的程序不能在主机上运行，检测编译器时只编译成静态库
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 只在目标环境里找库和头文件，程序（如 Python、脚本）照常在主机上找
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
