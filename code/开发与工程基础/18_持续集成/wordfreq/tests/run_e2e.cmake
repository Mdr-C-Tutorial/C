# 由 ctest 调用：运行 wordfreq，把输出与期望文件逐字节比较。
# 参数：WORDFREQ 可执行文件路径，DATA 存放 sample.txt 与 sample.expected 的目录。

execute_process(
    COMMAND "${WORDFREQ}" -n 3 "${DATA}/sample.txt"
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE err
    RESULT_VARIABLE status
)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "wordfreq exited with status ${status}: ${err}")
endif()

# Windows 上标准输出是文本模式，行尾为 CRLF；去掉回车再比较
string(REPLACE "\r" "" actual "${actual}")
file(READ "${DATA}/sample.expected" expected)

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "output differs\n--- actual ---\n${actual}--- expected ---\n${expected}")
endif()

message(STATUS "sample: ok")
