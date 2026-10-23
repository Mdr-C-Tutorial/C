# firmware.cmake：STM32F103C8 固件的公共构建设置。
#
# 在工程的 CMakeLists.txt 中（project() 之后）include 本文件，然后调用：
#   stm32f103_firmware(<目标> <源文件>... [LINKER_SCRIPT <链接脚本>])
# 它会：
#   - 把给出的源文件和 common/startup_stm32f103.c 链接成 <目标>.elf；
#   - 使用统一的编译选项，默认链接脚本为 common/stm32f103c8.ld；
#   - 生成映射文件 <目标>.map；
#   - 构建后打印 arm-none-eabi-size 的结果，并生成 <目标>.bin 和 <目标>.hex。
# 调用之后仍可用 target_sources、target_include_directories、
# target_link_libraries 给这个目标追加内容。
# 讲解见教程 1.8 节「复位、启动与链接」。

include_guard(GLOBAL)

set(STM32F103_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}")

# 把 common/ 下的头文件、源文件与链接脚本复制到构建目录的 emb_common/ 子目录。
# 这样编译和链接命令里只出现构建目录下的路径，不依赖源码树的位置。
set(STM32F103_COMMON_BIN_DIR "${CMAKE_BINARY_DIR}/emb_common")
file(MAKE_DIRECTORY "${STM32F103_COMMON_BIN_DIR}")
file(GLOB _stm32_common_files
    "${STM32F103_COMMON_DIR}/*.h"
    "${STM32F103_COMMON_DIR}/*.c"
    "${STM32F103_COMMON_DIR}/*.ld")
if(_stm32_common_files)
    file(COPY ${_stm32_common_files} DESTINATION "${STM32F103_COMMON_BIN_DIR}")
endif()

# 所有固件目标共用的编译与链接选项
add_library(stm32f103_options INTERFACE)
target_compile_definitions(stm32f103_options INTERFACE STM32F103xB)
target_include_directories(stm32f103_options INTERFACE
    "${STM32F103_COMMON_BIN_DIR}"
    "${STM32F103_COMMON_DIR}")
target_compile_options(stm32f103_options INTERFACE
    -std=c11 -mcpu=cortex-m3 -mthumb
    -O2 -g -Wall -Wextra
    -ffunction-sections -fdata-sections)
target_link_options(stm32f103_options INTERFACE
    -mcpu=cortex-m3 -mthumb
    -nostartfiles
    --specs=nano.specs --specs=nosys.specs
    -Wl,--gc-sections)

# common/ 下的源文件在构建目录的 emb_common/ 子目录里编译。
add_subdirectory("${STM32F103_COMMON_DIR}" "${STM32F103_COMMON_BIN_DIR}")

function(stm32f103_firmware target)
    cmake_parse_arguments(PARSE_ARGV 1 FW "" "LINKER_SCRIPT" "")
    if(NOT FW_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "stm32f103_firmware(${target})：没有给出源文件")
    endif()
    if(FW_LINKER_SCRIPT)
        get_filename_component(linker_script "${FW_LINKER_SCRIPT}" ABSOLUTE)
    else()
        set(linker_script "${STM32F103_COMMON_BIN_DIR}/stm32f103c8.ld")
    endif()

    add_executable(${target} ${FW_UNPARSED_ARGUMENTS})
    set_target_properties(${target} PROPERTIES
        SUFFIX ".elf"
        LINK_DEPENDS "${linker_script}")
    target_link_libraries(${target} PRIVATE stm32f103_options stm32f103_startup)
    target_link_options(${target} PRIVATE
        "-T${linker_script}"
        "-Wl,-Map=$<TARGET_FILE_DIR:${target}>/${target}.map")

    # 在 .elf 所在目录里用相对文件名执行
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_SIZE} ${target}.elf
        COMMAND ${CMAKE_OBJCOPY} -O binary ${target}.elf ${target}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${target}.elf ${target}.hex
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>"
        VERBATIM)
endfunction()
