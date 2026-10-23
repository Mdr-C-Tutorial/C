# 最小示例：两个 .c 文件加一个共用头文件。
# 在项目根目录执行 make。

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

hello: main.o greet.o
	$(CC) -o $@ $^

main.o: main.c greet.h
	$(CC) $(CFLAGS) -c main.c -o $@

greet.o: greet.c greet.h
	$(CC) $(CFLAGS) -c greet.c -o $@

clean:
	$(RM) hello hello.exe main.o greet.o
