cmake_minimum_required(VERSION 3.20)

project(mdr_server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MDR_SERVER_WERROR "Treat compiler warnings as errors" ON)
set(MDR_SERVER_TEST_PORT 20080 CACHE STRING "TCP port used by the integration test")

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if (MDR_SERVER_WERROR)
        add_compile_options(-Werror)
    endif ()
endif ()

# 可移植的核心：缓冲区、HTTP 解析与格式化、空闲定时器队列。
# 只用标准 C，在 Windows（MinGW）上也能编译和做单元测试。
add_library(mdr_server_core STATIC
    src/buffer.c
    src/http.c
    src/timer.c
)
target_include_directories(mdr_server_core PUBLIC include)

add_executable(unit_test tests/unit_test.c)
target_link_libraries(unit_test PRIVATE mdr_server_core)

enable_testing()
add_test(NAME unit COMMAND unit_test)

# 服务器本体依赖 epoll、signalfd 和 accept4，只在 Linux 上构建。
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(mdr_server
        src/main.c
        src/server.c
        src/connection.c
    )
    target_link_libraries(mdr_server PRIVATE mdr_server_core)

    add_executable(integration_test tests/integration_test.c)

    add_test(NAME integration
        COMMAND integration_test $<TARGET_FILE:mdr_server> ${MDR_SERVER_TEST_PORT})
    set_tests_properties(integration PROPERTIES TIMEOUT 60)
else ()
    message(STATUS "Not on Linux: building only the portable core and unit tests")
endif ()
