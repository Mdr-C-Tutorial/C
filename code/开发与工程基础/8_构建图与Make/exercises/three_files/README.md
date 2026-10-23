# three_files：习题 20801 的起点项目

三个源文件和一个共用头文件，没有构建文件：

```text
three_files/
├── calc.h       两个函数的声明
├── sum.c        calc_sum 的实现
├── product.c    calc_product 的实现
└── main.c       命令行程序，两个参数是整数 A 和 B
```

按仓库的代码规范，可以这样手工构建：

```sh
gcc -std=c17 -Wall -Wextra -c main.c -o main.o
gcc -std=c17 -Wall -Wextra -c sum.c -o sum.o
gcc -std=c17 -Wall -Wextra -c product.c -o product.o
gcc main.o sum.o product.o -o calc
./calc 6 7
```

```text
sum: 13
product: 42
```

习题要求为它写一个 Makefile：模式的编译规则、`build/` 存放产物、`all` 与 `clean` 目标，
并在分别 `touch` 每个源文件后观察 make 执行了哪些命令。
