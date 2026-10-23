cmake_minimum_required(VERSION 3.20)
project(exti_button C)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

stm32f103_firmware(exti_button main.c)
target_link_libraries(exti_button PRIVATE stm32f103_common)
