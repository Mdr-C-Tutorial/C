cmake_minimum_required(VERSION 3.20)
project(ch3_4_frame_ringbuf_fsm LANGUAGES C)

set(CH3_3_DIR "${CMAKE_CURRENT_LIST_DIR}/../3_3_轮询_中断_DMA与非阻塞驱动")

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    stm32f103_firmware(uart_frame_demo
        uart_frame_demo.c
        ringbuf.c
        crc16.c
        frame.c
        "${CH3_3_DIR}/uart_dma.c"
        "${CH3_3_DIR}/dma_rx_pos.c")
    target_include_directories(uart_frame_demo PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CH3_3_DIR}")
else()
    add_executable(test_ringbuf
        test_ringbuf.c
        ringbuf.c)
    target_compile_options(test_ringbuf PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(test_ringbuf PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

    add_executable(test_frame
        test_frame.c
        ringbuf.c
        crc16.c
        frame.c)
    target_compile_options(test_frame PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(test_frame PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()
