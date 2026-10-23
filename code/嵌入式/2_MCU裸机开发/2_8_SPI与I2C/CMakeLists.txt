cmake_minimum_required(VERSION 3.20)

set(EMB_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${EMB_COMMON_DIR}/arm-none-eabi.cmake")
endif()

project(ch2_8_spi_i2c LANGUAGES C)

include("${EMB_COMMON_DIR}/firmware.cmake")

stm32f103_firmware(w25q_id w25q_id/main.c spi.c)
target_link_libraries(w25q_id PRIVATE stm32f103_common)

stm32f103_firmware(eeprom_rw eeprom_rw/main.c soft_i2c.c)
target_link_libraries(eeprom_rw PRIVATE stm32f103_common)