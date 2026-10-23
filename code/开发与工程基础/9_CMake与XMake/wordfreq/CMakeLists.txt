cmake_minimum_required(VERSION 3.20)
project(wordfreq VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(wf STATIC
    src/token.c
    src/counter.c
    src/report.c
)
target_include_directories(wf PUBLIC include)
target_compile_options(wf PRIVATE -Wall -Wextra)

# MinGW 的 -Wformat 不认识 %zu（运行结果正确），加上这个宏改用 UCRT 的格式化函数
if(MINGW)
    target_compile_definitions(wf PUBLIC __USE_MINGW_ANSI_STDIO=1)
endif()

add_executable(wordfreq app/main.c)
target_link_libraries(wordfreq PRIVATE wf)
target_compile_options(wordfreq PRIVATE -Wall -Wextra)
