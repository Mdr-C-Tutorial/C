cmake_minimum_required(VERSION 3.20)
project(embedded_testing C)

# 6.3 节的示例：主机单元测试、链接替换、目标上运行与 HIL。
# 主机目标默认构建；交叉编译时额外构建测试固件。

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMSIS_DIR "" CACHE PATH "CMSIS 与 cmsis_device_f1 所在的根目录")

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")
    if(NOT CMSIS_DIR)
        message(FATAL_ERROR "交叉编译需要 -DCMSIS_DIR=<CMSIS 头文件根目录>")
    endif()

    # 同一组测试编进测试固件，结果从 UART 输出
    stm32f103_firmware(test_fw_target
        target_test/target_init.c
        fake_i2c/mpu6050.c
        fake_i2c/test_mpu6050.c)
    target_include_directories(test_fw_target PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/common"
        "${CMAKE_CURRENT_LIST_DIR}/fake_i2c"
        "${CMSIS_DIR}/CMSIS/Core/Include"
        "${CMSIS_DIR}/cmsis_device_f1/Include")
else()
    add_compile_options(-Wall -Wextra)
    add_library(test_assert INTERFACE)
    target_include_directories(test_assert INTERFACE "${CMAKE_CURRENT_LIST_DIR}/common")

    # 示例一：伪造寄存器
    add_executable(test_uart fake_usart/uart_byte.c fake_usart/test_uart_byte.c)
    target_link_libraries(test_uart PRIVATE test_assert)

    # 示例二：伪造总线
    add_executable(test_mpu6050 fake_i2c/mpu6050.c fake_i2c/test_mpu6050.c)
    target_link_libraries(test_mpu6050 PRIVATE test_assert)

    # 链接替换：测试构建链接 fake_tick.c 而不是真实的 tick 实现
    add_executable(test_blink
        link_replace/app_logic.c
        link_replace/fake_tick.c
        link_replace/test_blink.c)
    target_include_directories(test_blink PRIVATE "${CMAKE_CURRENT_LIST_DIR}/link_replace")
    target_link_libraries(test_blink PRIVATE test_assert)

    # 覆盖率：gcc --coverage
    add_executable(test_mpu6050_cov fake_i2c/mpu6050.c fake_i2c/test_mpu6050.c)
    target_link_libraries(test_mpu6050_cov PRIVATE test_assert)
    target_compile_options(test_mpu6050_cov PRIVATE --coverage)
    target_link_options(test_mpu6050_cov PRIVATE --coverage)

    enable_testing()
    add_test(NAME uart COMMAND test_uart)
    add_test(NAME mpu6050 COMMAND test_mpu6050)
    add_test(NAME blink COMMAND test_blink)
endif()