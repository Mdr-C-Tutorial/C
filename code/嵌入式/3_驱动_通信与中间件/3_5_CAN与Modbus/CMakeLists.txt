cmake_minimum_required(VERSION 3.20)
project(ch3_5_can_modbus LANGUAGES C)

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")

    stm32f103_firmware(can_loopback
        can_loopback/main.c
        can_loopback/can_drv.c)
    target_include_directories(can_loopback PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/can_loopback")

    stm32f103_firmware(modbus_rtu_fw
        modbus_rtu_fw/main.c
        modbus/crc16_modbus.c
        modbus/rtu_slave.c)
    target_include_directories(modbus_rtu_fw PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}")
else()
    add_executable(bit_timing_calc
        bit_timing_calc.c)
    target_compile_options(bit_timing_calc PRIVATE -std=c17 -Wall -Wextra)

    add_executable(test_rtu_slave
        modbus/test_rtu_slave.c
        modbus/crc16_modbus.c
        modbus/rtu_slave.c)
    target_compile_options(test_rtu_slave PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(test_rtu_slave PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/modbus")
endif()
