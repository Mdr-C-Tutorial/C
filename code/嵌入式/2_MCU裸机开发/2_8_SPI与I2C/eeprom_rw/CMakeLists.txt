cmake_minimum_required(VERSION 3.20)
project(eeprom_rw C)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

stm32f103_firmware(eeprom_rw main.c ../soft_i2c.c)
target_link_libraries(eeprom_rw PRIVATE stm32f103_common)
