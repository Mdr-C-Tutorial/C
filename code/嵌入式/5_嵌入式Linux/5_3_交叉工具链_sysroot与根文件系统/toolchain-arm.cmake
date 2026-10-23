# 交叉编译用的工具链文件。
#
# 用法：
#   cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=toolchain-arm.cmake
#
# 工具链不在 PATH 里时，把 CMAKE_C_COMPILER 与 CMAKE_CXX_COMPILER 改成绝对路径，
# 例如 /opt/arm/bin/arm-linux-gnueabihf-gcc。

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Buildroot 的 output/staging 换成自己机器上的路径。
# 用普通工具链加 --sysroot 时也写在这里。
if(DEFINED ENV{ARM_SYSROOT})
    set(CMAKE_SYSROOT $ENV{ARM_SYSROOT})
endif()

# 程序（pkg-config 之类）只在宿主机找，库与头文件只在 sysroot 找。
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)