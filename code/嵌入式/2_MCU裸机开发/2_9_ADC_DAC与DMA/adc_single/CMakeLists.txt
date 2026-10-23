cmake_minimum_required(VERSION 3.20)

set(EMB_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../common")
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${EMB_COMMON_DIR}/arm-none-eabi.cmake")
endif()

project(adc_single LANGUAGES C)

include("${EMB_COMMON_DIR}/firmware.cmake")

stm32f103_firmware(adc_single main.c)
target_link_libraries(adc_single PRIVATE stm32f103_common)
