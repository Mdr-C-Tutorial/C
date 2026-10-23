# 5.1 嵌入式 Linux 与系统编程边界：配套代码

只有一个用于观察 ELF 头的程序：

```text
gcc -std=c17 -Wall -Wextra hello.c -o hello_native
arm-linux-gnueabihf-gcc -std=c17 -Wall -Wextra -s hello.c -o hello_arm
readelf -h hello_native
readelf -h hello_arm
```

没有装 Arm 官方工具链时，clang 也能做同样的事：

```text
clang --target=arm-linux-gnueabihf -std=c17 -Wall -Wextra -O2 -s hello.c -o hello_arm
```

交叉编译器来自所选的工具链（Arm 官方、Buildroot 或发行版的交叉包），本目录不附带。ARM 版可以在 `file` 与 `readelf -h` 里看到 `ELF32`、`Machine: ARM` 与 `hard-float ABI`；把它放到 x86 机器上运行会得到 `Exec format error`。
