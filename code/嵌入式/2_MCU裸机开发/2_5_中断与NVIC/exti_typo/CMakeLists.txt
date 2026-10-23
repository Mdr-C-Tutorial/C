cmake_minimum_required(VERSION 3.20)
project(exti_typo C)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

stm32f103_firmware(exti_typo main.c)
target_link_libraries(exti_typo PRIVATE stm32f103_common)
