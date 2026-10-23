cmake_minimum_required(VERSION 3.20)
project(ch3_3_uart_nonblock LANGUAGES C)

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    stm32f103_firmware(uart_nonblock_demo
        main.c
        uart_it.c
        uart_dma.c
        dma_rx_pos.c)
    target_include_directories(uart_nonblock_demo PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
else()
    add_executable(test_dma_rx_pos
        test_dma_rx_pos.c
        dma_rx_pos.c)
    target_compile_options(test_dma_rx_pos PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(test_dma_rx_pos PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()
