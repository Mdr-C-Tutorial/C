cmake_minimum_required(VERSION 3.20)
project(cdc_echo LANGUAGES C)

set(TINYUSB_DIR "" CACHE PATH "TinyUSB 0.21.0 源码根目录")
if(NOT TINYUSB_DIR OR NOT EXISTS "${TINYUSB_DIR}/src/tusb.h")
    message(FATAL_ERROR "请通过 -DTINYUSB_DIR=<路径> 指定 TinyUSB 0.21.0 源码目录")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

set(CH2_3_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../2_MCU裸机开发/2_3_时钟与复位")

stm32f103_firmware(cdc_echo
    main.c
    usb_descriptors.c
    "${CH2_3_DIR}/clock.c"
    "${TINYUSB_DIR}/src/tusb.c"
    "${TINYUSB_DIR}/src/common/tusb_fifo.c"
    "${TINYUSB_DIR}/src/device/usbd.c"
    "${TINYUSB_DIR}/src/class/cdc/cdc_device.c"
    "${TINYUSB_DIR}/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c"
    "${TINYUSB_DIR}/src/portable/st/stm32_fsdev/fsdev_common.c")

target_compile_definitions(cdc_echo PRIVATE asm=__asm)

target_include_directories(cdc_echo PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CH2_3_DIR}"
    "${TINYUSB_DIR}/src")
