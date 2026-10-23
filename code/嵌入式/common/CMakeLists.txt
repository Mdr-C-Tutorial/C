# common/CMakeLists.txt：由 firmware.cmake 用 add_subdirectory 加入，不单独构建。
# 讲解见教程 1.8 节「复位、启动与链接」。

# 启动文件编成对象库：链接到固件目标时，它的目标文件直接进入链接，
# 不会像静态库成员那样因为「没有被引用」而被跳过。
add_library(stm32f103_startup OBJECT "${CMAKE_CURRENT_BINARY_DIR}/startup_stm32f103.c")
target_link_libraries(stm32f103_startup PRIVATE stm32f103_options)

# 后续小节陆续加进来的公共模块。还没写的文件跳过，所以只建立骨架时也能构建。
set(_stm32_common_sources "")
foreach(_module clock tick console retarget fault)
    if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${_module}.c")
        list(APPEND _stm32_common_sources "${CMAKE_CURRENT_BINARY_DIR}/${_module}.c")
    endif()
endforeach()

# 静态库只拉取被引用的目标文件，用不到的模块不进固件
if(_stm32_common_sources)
    add_library(stm32f103_common STATIC ${_stm32_common_sources})
    target_link_libraries(stm32f103_common PUBLIC stm32f103_options)
endif()