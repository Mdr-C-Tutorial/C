cmake_minimum_required(VERSION 3.20)
project(diagnostics C)

# 6.2 节的示例：日志、断言、诊断记录、存储与栈扫描。
# 主机产物默认全部构建；交叉编译时只构建目标侧的对象检查。

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_CROSSCOMPILING)
    include("${CMAKE_CURRENT_LIST_DIR}/../../common/firmware.cmake")

    # 目标侧的日志、诊断记录与存储，只做编译检查
    add_library(diag_target OBJECT
        diag/diag_record.c
        diag/diag_store.c
        log/log.c)
    target_link_libraries(diag_target PRIVATE stm32f103_options)
    target_include_directories(diag_target PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/diag"
        "${CMAKE_CURRENT_LIST_DIR}/log")

    # 日志的体积对比：只改 LOG_LEVEL，比较 text 列
    stm32f103_firmware(log_fw log/log_fw.c log/log.c)
    target_include_directories(log_fw PRIVATE "${CMAKE_CURRENT_LIST_DIR}/log")
else()
    add_compile_options(-Wall -Wextra)

    add_executable(log_demo log/log.c log/log_net.c log/log_demo.c)
    target_include_directories(log_demo PRIVATE "${CMAKE_CURRENT_LIST_DIR}/log")

    add_executable(assert_demo assert/assert_demo.c)
    target_include_directories(assert_demo PRIVATE "${CMAKE_CURRENT_LIST_DIR}/assert")

    add_executable(test_diag diag/diag_record.c diag/test_diag.c)
    target_include_directories(test_diag PRIVATE "${CMAKE_CURRENT_LIST_DIR}/diag")

    add_executable(test_store diag/diag_record.c diag/diag_store.c diag/test_store.c)
    target_include_directories(test_store PRIVATE "${CMAKE_CURRENT_LIST_DIR}/diag")

    add_executable(stackscan stackscan/stackscan.c)

    add_executable(diagdump diag/diagdump.c diag/diag_record.c)
    target_include_directories(diagdump PRIVATE "${CMAKE_CURRENT_LIST_DIR}/diag")
endif()