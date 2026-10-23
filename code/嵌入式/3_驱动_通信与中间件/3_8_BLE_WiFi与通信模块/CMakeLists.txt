cmake_minimum_required(VERSION 3.20)
project(ch3_8_ble_wifi_at LANGUAGES C)

set(CH3_4_DIR "${CMAKE_CURRENT_LIST_DIR}/../3_4_数据帧_环形缓冲区与状态机")

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    stm32f103_firmware(at_wifi_demo
        at_wifi_demo.c
        at_parser.c
        "${CH3_4_DIR}/ringbuf.c")
    target_include_directories(at_wifi_demo PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CH3_4_DIR}")
else()
    add_executable(test_at_parser
        test_at_parser.c
        at_parser.c
        "${CH3_4_DIR}/ringbuf.c")
    target_compile_options(test_at_parser PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(test_at_parser PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CH3_4_DIR}")

    add_executable(adv_decode
        adv_decode.c)
    target_compile_options(adv_decode PRIVATE -std=c17 -Wall -Wextra)
endif()
