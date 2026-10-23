cmake_minimum_required(VERSION 3.20)
project(button C)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

stm32f103_firmware(button
    main.c
    "${CMAKE_CURRENT_LIST_DIR}/../gpio/gpio.c")
