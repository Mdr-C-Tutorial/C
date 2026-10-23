# A.7 的示例。MinGW-w64 GCC 13.2（UCRT），x64。
#
#   mingw32-make            构建 DLL、使用方、插件与探测程序
#   mingw32-make clean      删除编译产物
#
# cast_warn.c 故意做一次直接转换，编译时会出现 -Wcast-function-type 警告，
# 那是正文要展示的现象，不是构建失败。

CC = gcc
CFLAGS = -std=c17 -Wall -Wextra

PROGS := use_load.exe use_runtime.exe refcount.exe counter_use.exe \
         plugin_host.exe trace_host.exe hang_host.exe \
         search_demo.exe cast_warn.exe arena_user.exe
DLLS := mdrmath.dll trace_dll.dll hang_dll.dll arena_dll.dll noexp.dll counter_dll.dll
PLUGINS := plugins/plugin_double.dll plugins/plugin_square.dll
HIJACK := hijack/version.dll hijack/hijack_demo.exe

all: $(DLLS) $(PLUGINS) $(PROGS) $(HIJACK)

mdrmath.dll: mdrmath.c mdrmath.h
	$(CC) $(CFLAGS) -shared -DMDRMATH_BUILD -o $@ mdrmath.c -Wl,--out-implib,libmdrmath.dll.a

# 加载时链接：链接导入库 libmdrmath.dll.a。
use_load.exe: use_load.c mdrmath.h mdrmath.dll
	$(CC) $(CFLAGS) -o $@ use_load.c -L. -lmdrmath

# 运行时链接：不需要导入库，只依赖 LoadLibraryW。
use_runtime.exe: use_runtime.c mdrmath.dll
	$(CC) $(CFLAGS) -o $@ use_runtime.c

refcount.exe: refcount.c mdrmath.dll
	$(CC) $(CFLAGS) -o $@ refcount.c

trace_dll.dll: trace_dll.c
	$(CC) $(CFLAGS) -shared -o $@ trace_dll.c

hang_dll.dll: hang_dll.c
	$(CC) $(CFLAGS) -shared -o $@ hang_dll.c

arena_dll.dll: arena_dll.c
	$(CC) $(CFLAGS) -shared -o $@ arena_dll.c -Wl,--out-implib,libarena.dll.a

noexp.dll: noexp.c
	$(CC) $(CFLAGS) -shared -o $@ noexp.c

counter_dll.dll: counter_dll.c
	$(CC) $(CFLAGS) -shared -o $@ counter_dll.c

counter_use.exe: counter_use.c counter_dll.dll
	$(CC) $(CFLAGS) -o $@ counter_use.c

plugins/plugin_double.dll: plugin_double.c
	mkdir -p plugins
	$(CC) $(CFLAGS) -shared -o $@ plugin_double.c

plugins/plugin_square.dll: plugin_square.c
	mkdir -p plugins
	$(CC) $(CFLAGS) -shared -o $@ plugin_square.c

hijack/version.dll: fake_version.c
	mkdir -p hijack
	$(CC) $(CFLAGS) -shared -o $@ fake_version.c

plugin_host.exe: plugin_host.c plugin.h
	$(CC) $(CFLAGS) -municode -o $@ plugin_host.c

trace_host.exe: trace_host.c
	$(CC) $(CFLAGS) -municode -o $@ trace_host.c

hang_host.exe: hang_host.c
	$(CC) $(CFLAGS) -o $@ hang_host.c

search_demo.exe: search_demo.c
	$(CC) $(CFLAGS) -municode -o $@ search_demo.c

cast_warn.exe: cast_warn.c
	$(CC) $(CFLAGS) -o $@ cast_warn.c

arena_user.exe: arena_user.c arena_dll.dll
	$(CC) $(CFLAGS) -o $@ arena_user.c

hijack/hijack_demo.exe: hijack_demo.c hijack/version.dll
	mkdir -p hijack
	$(CC) $(CFLAGS) -o $@ hijack_demo.c

clean:
	-del /Q $(PROGS) $(DLLS) $(PLUGINS) $(HIJACK) libmdrmath.dll.a libarena.dll.a 2>nul

.PHONY: all clean