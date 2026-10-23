cmake_minimum_required(VERSION 3.20)
project(w25q_id C)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

stm32f103_firmware(w25q_id main.c ../spi.c)
target_link_libraries(w25q_id PRIVATE stm32f103_common)
