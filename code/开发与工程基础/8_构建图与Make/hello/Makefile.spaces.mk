# 反例：配方行用空格缩进，make 报 missing separator。
# 用 make -f Makefile.spaces.mk 运行。

hello: main.o greet.o
    gcc -o $@ $^

main.o: main.c greet.h
    gcc -std=c17 -Wall -Wextra -c main.c -o $@

greet.o: greet.c greet.h
    gcc -std=c17 -Wall -Wextra -c greet.c -o $@
