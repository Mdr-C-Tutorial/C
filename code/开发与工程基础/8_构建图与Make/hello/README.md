# hello：最小 Makefile 示例（第 8 章第 2、3 节）

两个源文件加一个共用头文件：

```text
hello/
├── main.c              调用 greet
├── greet.c             greet 的实现
├── greet.h             声明
├── Makefile            链接、两条编译规则和 clean
└── Makefile.spaces.mk  反例：配方行用空格缩进，make 报 missing separator
```

```sh
make
./hello
```

```text
Hello, world!
```

再次运行 `make`，Linux 上显示 `make: 'hello' is up to date.`；`touch greet.c` 之后只重编
`greet.o` 再链接，`touch greet.h` 会重编两个目标文件。

Windows 上有一点不同：链接器生成的是 `hello.exe`，而规则里的目标叫 `hello`，make 找不到
这个文件，于是第二次 `make` 仍会重新链接一遍。程序不受影响，`./hello` 在 bash 里照常运行。
本章的 `../wordfreq/Makefile` 也没有处理这个后缀；正文第 9 节把平台差异列为 Make 的局限，
第 16 章再讲按平台区分构建文件。

`Makefile.spaces.mk` 只用于演示错误，用 `make -f Makefile.spaces.mk` 运行：

```text
Makefile.spaces.mk:5: *** missing separator.  Stop.
```
