# 第 11 章的注入缺陷

`token_lowercase_starts_at_1.c` 是 `src/token.c` 的一个变体：小写转换的循环从下标 1 开始，
下标 0 的字母保持原样。在 `wordfreq/` 目录下用它替换 `src/token.c`，重新构建并运行测试：

```sh
cp ../bugs/token_lowercase_starts_at_1.c src/token.c
cmake --build build
ctest --test-dir build --output-on-failure
```

真实输出（Windows，MinGW-w64 GCC 13.2）：

```text
    Start 1: token
1/3 Test #1: token ............................***Failed    0.03 sec
D:/Projects/C/code/开发与工程基础/11_自动化测试/wordfreq/tests/test_token.c:44: word is "The", expected "the"
D:/Projects/C/code/开发与工程基础/11_自动化测试/wordfreq/tests/test_token.c:46: word is "The", expected "the"
test_token: 27 checks, 2 failed

    Start 2: counter
2/3 Test #2: counter ..........................   Passed    0.03 sec
    Start 3: e2e
3/3 Test #3: e2e ..............................***Failed    0.06 sec
CMake Error at D:/Projects/C/code/开发与工程基础/11_自动化测试/wordfreq/tests/run_e2e.cmake:19 (message):
  output differs

  --- actual ---

        2 The
        2 on
        2 sat

  --- expected ---

        4 the
        2 on
        2 sat
```

失败信息同时给出检查所在的文件与行、实际值和期望值。端到端测试也失败了，因为样例文本
第一行的 `The` 没有被转成小写。修复的办法是在收集字母时顺手转换，或者让转换循环从下标 0
开始。测试与修复应放在同一次提交里。
