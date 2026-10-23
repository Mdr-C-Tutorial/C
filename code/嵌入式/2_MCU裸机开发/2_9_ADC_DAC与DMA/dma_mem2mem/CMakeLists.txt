cmake_minimum_required(VERSION 3.20)

set(EMB_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../common")
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${EMB_COMMON_DIR}/arm-none-eabi.cmake")
endif()

project(dma_mem2mem LANGUAGES C)

include("${EMB_COMMON_DIR}/firmware.cmake")

stm32f103_firmware(dma_mem2mem main.c)
target_link_libraries(dma_mem2mem PRIVATE stm32f103_common)
