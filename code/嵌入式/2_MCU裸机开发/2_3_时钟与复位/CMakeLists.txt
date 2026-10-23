cmake_minimum_required(VERSION 3.20)
project(clock_demo C)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

stm32f103_firmware(clock_demo main.c clock.c)
