cmake_minimum_required(VERSION 3.20)
project(storage_and_fs C)

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    stm32f103_firmware(storage_fw
        main_fw.c
        flash_stm32.c
        flash_cfg.c
        at24c02.c
        w25q.c
        sdspi.c
        ../../2_MCU裸机开发/2_8_SPI与I2C/spi.c
        ../../2_MCU裸机开发/2_8_SPI与I2C/soft_i2c.c
        LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/stm32f103c8_config.ld")
    target_include_directories(storage_fw PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/../../2_MCU裸机开发/2_8_SPI与I2C")
else()
    add_executable(test_flash_cfg
        test_flash_cfg.c
        flash_cfg.c)
    target_compile_options(test_flash_cfg PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(test_flash_cfg PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

    add_executable(crc_check
        crc_check.c
        flash_cfg.c)
    target_compile_options(crc_check PRIVATE -std=c17 -Wall -Wextra)
    target_include_directories(crc_check PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
endif()
