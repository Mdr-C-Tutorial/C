# sqlite3 自定义函数与 VFS 垫片

本目录是《6.4 SQLite：调试、测试与修改任务》的配套代码。合并源码不入库：从 <https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip> 下载并解压，构建时用 `SQLITE_DIR` 指向解压出的目录。

```text:no-line-numbers
SQLite/
├── CMakeLists.txt
├── test_users.c        # C API 写的测试，后面两节的修改都用它验收
├── mdr_reverse.c       # 应用定义函数 mdr_reverse(text)
├── mdrreverse.c        # 同一个函数做成可加载扩展（DLL）
├── static_result.c     # SQLITE_STATIC 与 sqlite3_free 两种结果所有权的对照
└── vfs/
    ├── vfslog.h
    ├── vfslog.c        # 记录读写的 VFS 垫片
    └── vfsdemo.c       # 用垫片观察提交顺序与 WAL 帧
```

## 构建

```powershell
cmake -S code/项目/SQLite -B code/项目/SQLite/build -G Ninja -DSQLITE_DIR=C:/sqlite/sqlite-amalgamation-3460100
cmake --build code/项目/SQLite/build
```

`SQLITE_DIR` 必须同时包含 `sqlite3.c`、`sqlite3.h`、`sqlite3ext.h`。在构建目录里运行：

```text
test_users.exe
mdr_reverse.exe
mdr_reverse.exe explain
static_result.exe
vfsdemo.exe rollback demo.db
vfsdemo.exe wal demo.db
```

可加载扩展在 Windows 上是 `mdrreverse.dll`，用命令行工具的 `.load ./mdrreverse.dll` 载入。

## 调试版命令行工具

`CMakeLists.txt` 里的 `sqlite3d` 目标按页面第 1 节给出的六个 `-D` 选项编译合并源码：

```text
sqlite3d.exe --version
```