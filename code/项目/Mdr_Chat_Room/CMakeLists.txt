cmake_minimum_required(VERSION 3.20)

project(mdr_chat_room C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CHAT_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if (MSVC)
    set(chat_warnings /W4)
else ()
    set(chat_warnings -Wall -Wextra -Wpedantic -Werror)
endif ()

if (CHAT_SANITIZE AND NOT MSVC)
    # UBSan 默认只打印报告、程序照常运行，测试仍判为通过；
    # -fno-sanitize-recover=all 让它像 ASan 一样一报错就终止程序。
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all
                        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif ()

# 协议编解码和聊天室规则只用 ISO C，任何平台都能编译、测试。
add_library(chat_core STATIC
    src/protocol.c
    src/room.c
)
target_include_directories(chat_core PUBLIC include)
target_compile_options(chat_core PRIVATE ${chat_warnings})

enable_testing()

foreach (name IN ITEMS protocol room)
    add_executable(test_${name} tests/test_${name}.c)
    target_link_libraries(test_${name} PRIVATE chat_core)
    target_compile_options(test_${name} PRIVATE ${chat_warnings})
    add_test(NAME ${name} COMMAND test_${name})
endforeach ()

# 服务器和客户端用到 POSIX 套接字和 poll()，只在类 Unix 系统上构建。
if (UNIX)
    add_executable(chat_server src/server.c src/net.c)
    target_link_libraries(chat_server PRIVATE chat_core)
    target_compile_options(chat_server PRIVATE ${chat_warnings})

    add_executable(chat_client src/client.c src/net.c)
    target_link_libraries(chat_client PRIVATE chat_core)
    target_compile_options(chat_client PRIVATE ${chat_warnings})

    find_program(BASH_PROGRAM bash)
    if (BASH_PROGRAM)
        add_test(NAME integration
            COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.sh
                    $<TARGET_FILE_DIR:chat_server>)
    endif ()
endif ()
