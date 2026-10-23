cmake_minimum_required(VERSION 3.20)

set(EMB_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${EMB_COMMON_DIR}/arm-none-eabi.cmake")
endif()

project(ch2_9_adc_dac_dma LANGUAGES C)

include("${EMB_COMMON_DIR}/firmware.cmake")

foreach(target adc_single dma_mem2mem adc_scan_dma adc_timer_trigger pwm_dac)
    stm32f103_firmware(${target} "${target}/main.c")
    target_link_libraries(${target} PRIVATE stm32f103_common)
endforeach()
