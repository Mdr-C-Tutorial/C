cmake_minimum_required(VERSION 3.20)
project(debug_demo C)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

stm32f103_firmware(debug_demo main.c)
