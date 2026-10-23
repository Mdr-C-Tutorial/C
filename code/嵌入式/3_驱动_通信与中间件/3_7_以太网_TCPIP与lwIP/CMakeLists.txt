cmake_minimum_required(VERSION 3.20)
project(ch3_7_ethernet_lwip LANGUAGES C)

# lwIP 源码目录（默认要求传入 -DLWIP_DIR=/path/to/lwip）
set(LWIP_DIR "" CACHE PATH "Path to lwIP STABLE-2_2_1_RELEASE root directory")

if(LWIP_DIR AND EXISTS "${LWIP_DIR}/src/core/init.c")
    file(GLOB LWIP_CORE_SRCS
        "${LWIP_DIR}/src/core/*.c"
        "${LWIP_DIR}/src/core/ipv4/*.c"
        "${LWIP_DIR}/src/netif/ethernet.c")
endif()

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    set(CH2_3_DIR "${CMAKE_CURRENT_LIST_DIR}/../../2_MCU裸机开发/2_3_时钟与复位")
    set(CH2_6_DIR "${CMAKE_CURRENT_LIST_DIR}/../../2_MCU裸机开发/2_6_定时器_PWM与系统节拍")
    set(CH2_7_DIR "${CMAKE_CURRENT_LIST_DIR}/../../2_MCU裸机开发/2_7_UART")

    if(LWIP_CORE_SRCS)
        stm32f103_firmware(fw_enc28j60_echo
            fw_enc28j60_echo/main.c
            fw_enc28j60_echo/enc28j60.c
            fw_enc28j60_echo/ethernetif.c
            fw_enc28j60_echo/echo_server.c
            "${CH2_3_DIR}/clock.c"
            "${CH2_6_DIR}/tick.c"
            "${CH2_7_DIR}/console.c"
            ${LWIP_CORE_SRCS})
        target_include_directories(fw_enc28j60_echo PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/fw_enc28j60_echo"
            "${CMAKE_CURRENT_LIST_DIR}/lwip_host_echo"
            "${CH2_3_DIR}"
            "${CH2_6_DIR}"
            "${CH2_7_DIR}"
            "${LWIP_DIR}/src/include")
    endif()
else()
    add_executable(frame_parse frame_parse.c)
    target_compile_options(frame_parse PRIVATE -std=c17 -Wall -Wextra)

    if(LWIP_CORE_SRCS)
        add_executable(lwip_host_echo
            lwip_host_echo/main.c
            lwip_host_echo/echo_server.c
            ${LWIP_CORE_SRCS})
        target_compile_options(lwip_host_echo PRIVATE -std=c17 -Wall -Wextra)
        target_include_directories(lwip_host_echo PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/lwip_host_echo"
            "${LWIP_DIR}/src/include")
    endif()
endif()
