# imud 的源码清单。
#
# 两种构建方式：
#   - 宿主机开发：自己准备 coreMQTT，用 -DCOREMQTT_DIR=... 指到源码根目录；
#   - Buildroot 里：coremqtt 包已经把头文件与静态库装进 sysroot，
#     直接找 libcoreMQTT.a 用，不再编一遍。
#
# 核心模块用 C17，-Wall -Wextra 无警告；coreMQTT 是第三方源码，
# 不参与我们自己的警告检查。

cmake_minimum_required(VERSION 3.16)
project(imud C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# sysroot 里有预编译的 coreMQTT 就直接用（Buildroot 的情况）
find_library(COREMQTT_LIBRARY NAMES coreMQTT)
find_path(COREMQTT_INCLUDE_DIR NAMES core_mqtt.h PATH_SUFFIXES coreMQTT)

set(COREMQTT_DIR "" CACHE PATH "coreMQTT 源码根目录（sysroot 里没有库时使用）")

if(COREMQTT_LIBRARY AND COREMQTT_INCLUDE_DIR)
    add_library(core_mqtt UNKNOWN IMPORTED)
    set_target_properties(core_mqtt PROPERTIES
        IMPORTED_LOCATION "${COREMQTT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${COREMQTT_INCLUDE_DIR}"
    )
elseif(COREMQTT_DIR)
    add_library(core_mqtt STATIC
        ${COREMQTT_DIR}/source/core_mqtt.c
        ${COREMQTT_DIR}/source/core_mqtt_serializer.c
        ${COREMQTT_DIR}/source/core_mqtt_state.c
    )
    target_include_directories(core_mqtt PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}          # 自己的 core_mqtt_config.h
        ${COREMQTT_DIR}/source/include
        ${COREMQTT_DIR}/source/interface
    )
    target_compile_options(core_mqtt PRIVATE -O2)
else()
    message(FATAL_ERROR
        "找不到 coreMQTT：给 -DCOREMQTT_DIR=路径，或让 sysroot 里有 libcoreMQTT.a")
endif()

add_executable(imud
    main.c
    config.c
    filter.c
    sample_io.c
    mqtt_glue.c
    transport_posix.c
    evl.c
)
target_include_directories(imud PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imud PRIVATE core_mqtt m)
target_compile_options(imud PRIVATE -Wall -Wextra)

add_executable(test_imud
    tests/test_imud.c
    config.c
    filter.c
    sample_io.c
)
target_include_directories(test_imud PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_imud PRIVATE m)
target_compile_options(test_imud PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME unit COMMAND test_imud)