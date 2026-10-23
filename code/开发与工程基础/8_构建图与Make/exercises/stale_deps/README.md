# stale_deps：习题 20802 的对照构建文件

`no_deps.mk` 与 `../../wordfreq/Makefile` 只差两处：`CPPFLAGS` 里没有 `-MMD -MP`，
末尾没有 `-include $(DEPS)`。用它构建时 make 只看 `.c` 文件的时间戳，改动头文件不会触发重编。

```sh
cp no_deps.mk ../../wordfreq/no_deps.mk
cd ../../wordfreq
make -f no_deps.mk
touch include/wf/counter.h
make -f no_deps.mk
```

最后一条命令的输出是：

```text
make: Nothing to be done for 'all'.
```

把 `CPPFLAGS` 改回 `-Iinclude -MMD -MP` 并在末尾加上 `-include $(DEPS)`（即换回同目录的
`Makefile`），`make clean` 后重新构建：`build/*.d` 才有内容，改动 `include/wf/counter.h`
会让 make 重编 `counter.o`、`report.o` 与 `main.o`。

注意：从没有依赖记录的构建目录直接换成带 `-MMD -MP` 的 Makefile 时，`.d` 文件还不存在，
第一次仍然不会重编；必须先 `make clean`，这个细节在正文第 5 节说明。
