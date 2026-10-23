# 2.1 源码阅读：kilo

kilo 是 antirez（Redis 的作者）写的一个终端文本编辑器，整个项目只有一个 C 文件，不到 1000 行有效代码（以 cloc 计）。它不依赖 ncurses，直接向终端发送 VT100 转义序列完成所有渲染，适合用来学习 C 程序如何直接控制终端。

本文先列出终端编辑器要解决的问题，再按 `kilo.c` 的代码分区阅读对应的实现。

> 源码仓库：<https://github.com/antirez/kilo>，BSD-2-Clause 许可。本文基于截至 2016 年的最新版本。

## 1. 一个终端编辑器需要做什么

在终端里运行 `vim` 或 `nano` 时，看到的是一整屏实时刷新的文字。终端在默认模式下按行缓冲输入并自动回显，程序要等用户按下回车才能读到整行文本；要在屏幕任意位置绘制文字，程序必须自己发送转义序列移动光标。因此一个终端编辑器至少需要解决五个问题：

1. **接管终端**：关闭终端的行缓冲和回显，让程序能逐字节读取用户的每次按键。
2. **读懂按键**：普通字符直接就是 ASCII，但方向键、Page Up 等特殊键会产生多字节的转义序列，需要解析。
3. **维护文档模型**：在内存里用合适的数据结构保存每一行文本，支持插入、删除、拆行、合并行。
4. **渲染屏幕**：每次按键后，根据文档模型和光标位置，重新计算整屏内容，用转义序列画出来。
5. **文件读写**：从磁盘读入文件、写回磁盘。

kilo 围绕这五个问题组织代码，外加两个增强功能：**增量搜索**和**语法高亮**。

## 2. 全局架构

### 2.1 一个全局状态

kilo 把编辑器的全部状态放在一个全局结构体里：

```c
struct editorConfig {
    int cx,cy;          /* 光标在屏幕上的 x, y 位置 */
    int rowoff;         /* 垂直滚动偏移 */
    int coloff;         /* 水平滚动偏移 */
    int screenrows;     /* 终端行数 */
    int screencols;     /* 终端列数 */
    int numrows;        /* 文件总行数 */
    int rawmode;        /* 终端是否处于 raw 模式 */
    erow *row;          /* 所有行的动态数组 */
    int dirty;          /* 文件是否被修改过 */
    char *filename;     /* 当前文件名 */
    char statusmsg[80]; /* 状态栏消息 */
    time_t statusmsg_time;
    struct editorSyntax *syntax; /* 当前语法高亮规则 */
};

static struct editorConfig E;
```

kilo 一次只编辑一个文件，所以用一个全局结构体保存全部状态。各函数直接通过 `E.cx`、`E.row` 等成员读写状态，不需要在函数间传递上下文指针。

### 2.2 主循环

`main` 函数只做初始化，然后进入主循环：

```c
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr,"Usage: kilo <filename>\n");
        exit(1);
    }

    initEditor();                           // 初始化全局状态
    editorSelectSyntaxHighlight(argv[1]);   // 根据文件扩展名选语法
    editorOpen(argv[1]);                    // 从磁盘读入文件
    enableRawMode(STDIN_FILENO);            // 接管终端
    editorSetStatusMessage(
        "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while(1) {
        editorRefreshScreen();              // 画屏幕
        editorProcessKeypress(STDIN_FILENO);// 读一次按键并处理
    }
    return 0;
}
```

主循环反复执行两步：按当前状态重绘整屏，然后读取并处理一次按键。

### 2.3 源码分区

kilo.c 用注释横线把代码分成七个区域，按在文件中的顺序依次是：

| 区域 | 行范围（约） | 职责 |
|:---|:---|:---|
| Syntax highlights DB | 143–198 | 语法高亮的**数据**：文件匹配规则和关键字表 |
| Low level terminal handling | 200–362 | 终端 raw 模式、按键读取、窗口尺寸 |
| Syntax highlight color scheme | 364–551 | 语法高亮的**算法**：逐字符状态机 |
| Editor rows implementation | 553–852 | 文档模型：行的增删改查、文件读写 |
| Terminal update | 854–1008 | 屏幕渲染：append buffer、刷屏 |
| Find mode | 1010–1107 | 增量搜索 |
| Editor events handling | 1109–1308 | 按键分发、光标移动、主循环 |

下面按自顶向下的顺序，从"用户按了一个键"开始，逐层展开。

## 3. 按键处理：从用户意图到编辑操作

### 3.1 按键分发

`editorProcessKeypress` 读取一次按键，然后用 `switch` 分发（省略了部分分支）：

```c
#define KILO_QUIT_TIMES 3
void editorProcessKeypress(int fd) {
    /* 文件已修改时，要求连按多次 Ctrl-Q 才真正退出 */
    static int quit_times = KILO_QUIT_TIMES;

    int c = editorReadKey(fd);
    switch(c) {
    case ENTER:         /* Enter */
        editorInsertNewline();
        break;
    case CTRL_C:        /* Ctrl-c */
        /* 忽略 Ctrl-C，避免轻易丢失对文件的修改 */
        break;
    case CTRL_Q:        /* Ctrl-q */
        /* 文件已保存则直接退出 */
        if (E.dirty && quit_times) {
            editorSetStatusMessage("WARNING!!! File has unsaved changes. "
                "Press Ctrl-Q %d more times to quit.", quit_times);
            quit_times--;
            return;
        }
        exit(0);
        break;
    case CTRL_S:        /* Ctrl-s */
        editorSave();
        break;
    case CTRL_F:
        editorFind(fd);
        break;
    case BACKSPACE:     /* Backspace */
    case CTRL_H:        /* Ctrl-h */
    case DEL_KEY:
        editorDelChar();
        break;
    /* PAGE_UP、PAGE_DOWN：转换为多次 editorMoveCursor（略） */
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    /* CTRL_L、ESC：不做处理（略） */
    default:
        editorInsertChar(c);
        break;
    }

    quit_times = KILO_QUIT_TIMES; /* 恢复初始值 */
}
```

值得注意的设计：

- **未保存退出保护**：`Ctrl-Q` 在文件有未保存的修改时不会直接退出，前 3 次只在状态栏显示警告并提前 `return`，第 4 次才退出。用一个 `static` 局部变量 `quit_times` 做倒计数，任何其他按键都会重置它。
- **`Ctrl-C` 被忽略**：编辑器不能因为用户按了 `Ctrl-C` 就丢失修改。
- **默认分支插入字符**：只要不是特殊键，就当作普通字符插入。

### 3.2 读取按键

`editorReadKey` 负责把终端的原始字节流翻译成逻辑按键。普通字符直接返回其 ASCII 值，但特殊键需要解析 VT100 转义序列：

```c
int editorReadKey(int fd) {
    int nread;
    char c, seq[3];
    while ((nread = read(fd,&c,1)) == 0);  // 等到有字节可读
    if (nread == -1) exit(1);

    while(1) {
        switch(c) {
        case ESC:
            // 尝试读后续字节，超时则当作单独的 ESC
            if (read(fd,seq,1) == 0) return ESC;
            if (read(fd,seq+1,1) == 0) return ESC;

            if (seq[0] == '[') {
                // ESC [ A → ARROW_UP, ESC [ B → ARROW_DOWN, ...
                // ESC [ 5 ~ → PAGE_UP, ESC [ 6 ~ → PAGE_DOWN, ...
            }
            break;
        default:
            return c;
        }
    }
}
```

关键技巧：终端的 raw 模式下 `VTIME` 被设为 1（100ms 超时）。如果用户按的是 `ESC` 键，后面不会跟更多字节，`read` 会超时返回 0，函数就知道这是一个单独的 ESC；如果是方向键（`ESC [ A`），后续字节会很快到达。

kilo 用 `enum KEY_ACTION` 把特殊键映射到 1000 以上的整数，和 ASCII 区间不重叠：

```c
enum KEY_ACTION {
    KEY_NULL = 0,
    CTRL_C = 3, CTRL_D = 4, CTRL_F = 6, /* ... */
    ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN,
    DEL_KEY, HOME_KEY, END_KEY, PAGE_UP, PAGE_DOWN
};
```

## 4. 终端控制层

### 4.1 Raw 模式

终端默认工作在"cooked 模式"（也叫 canonical 模式）：用户输入被行缓冲，按回车才交给程序；输入的字符会回显到屏幕；`Ctrl-C` 会产生信号。编辑器需要关掉这一切：

```c
int enableRawMode(int fd) {
    struct termios raw;

    if (E.rawmode) return 0;
    if (!isatty(STDIN_FILENO)) goto fatal;
    atexit(editorAtExit);
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;
    // 关闭：中断信号、CR-to-NL、奇偶校验、字符剥离、软件流控
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // 关闭：输出后处理（\n → \r\n 的自动转换）
    raw.c_oflag &= ~(OPOST);
    // 设置 8 位字符
    raw.c_cflag |= (CS8);
    // 关闭：回显、行缓冲、扩展功能、信号字符
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // VMIN = 0、VTIME = 1：有字节即返回，否则最多等待 100ms 后返回 0
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    E.rawmode = 1;
    return 0;
fatal:
    errno = ENOTTY;
    return -1;
}
```

`atexit(editorAtExit)` 注册了退出回调，确保无论程序如何退出，终端都会恢复到原始模式。这是使用 raw 模式的**必须**步骤——否则用户退出后终端就废了。

### 4.2 获取终端尺寸

kilo 需要知道终端有多少行多少列。首选 `ioctl(TIOCGWINSZ)`，如果失败就用转义序列兜底：

```c
int getWindowSize(int ifd, int ofd, int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // ioctl() 失败，改为向终端本身查询
        int orig_row, orig_col, retval;

        // 记下当前光标位置，以便稍后恢复
        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);
        if (retval == -1) goto failed;

        // 把光标移到右下角，再查询位置
        if (write(ofd,"\x1b[999C\x1b[999B",12) != 12) goto failed;
        retval = getCursorPosition(ifd,ofd,rows,cols);
        if (retval == -1) goto failed;

        // 恢复光标位置
        char seq[32];
        snprintf(seq,32,"\x1b[%d;%dH",orig_row,orig_col);
        if (write(ofd,seq,strlen(seq)) == -1) {
            /* 无法恢复，忽略 */
        }
        return 0;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

failed:
    return -1;
}
```

备用方案的原理：`ESC[999C` 把光标向右移 999 列（会停在右边界），`ESC[999B` 向下移 999 行（停在底边界），然后用 `ESC[6n` 查询光标当前位置——这个位置就是终端的尺寸。查询之前，kilo 先记下光标原来的位置，查询之后再把光标移回去。

kilo 还用 `signal(SIGWINCH, handleSigWinCh)` 监听终端窗口大小变化，收到信号后重新获取尺寸并刷屏。

## 5. 文档模型

### 5.1 行的表示

kilo 用 `erow`（editor row）结构表示一行文本：

```c
typedef struct erow {
    int idx;            /* 行号（从 0 开始） */
    int size;           /* 原始内容长度（不含 \0） */
    int rsize;          /* 渲染后长度 */
    char *chars;        /* 原始内容 */
    char *render;       /* 渲染后内容（Tab 展开为空格） */
    unsigned char *hl;  /* 每个渲染字符的高亮类型 */
    int hl_oc;          /* 该行末尾是否有未闭合的多行注释 */
} erow;
```

`chars` 保存文件中的原始字节，`render` 保存实际显示的内容：Tab 在文件里是一个字节，在屏幕上要展开为多个空格（kilo 按 8 列对齐）。`hl` 数组与 `render` 等长，记录每个字符应该用什么颜色渲染。

所有行存储在 `E.row` 动态数组里，用 `E.numrows` 记录行数。

### 5.2 行操作

kilo 提供了一组行操作函数，它们是所有编辑功能的基础：

| 函数 | 作用 |
|:---|:---|
| `editorInsertRow(at, s, len)` | 在位置 `at` 插入一行，后续行用 `memmove` 下移 |
| `editorDelRow(at)` | 删除位置 `at` 的行，后续行上移 |
| `editorRowInsertChar(row, at, c)` | 在行内位置 `at` 插入字符 |
| `editorRowDelChar(row, at)` | 删除行内位置 `at` 的字符 |
| `editorRowAppendString(row, s, len)` | 把字符串追加到行尾 |
| `editorUpdateRow(row)` | 根据 `chars` 重新生成 `render` 和 `hl` |

每个修改操作都会调用 `editorUpdateRow` 重新计算渲染内容和语法高亮，并递增 `E.dirty`。

### 5.3 高层编辑操作

建立在行操作之上的是面向用户的编辑函数：

**插入字符** `editorInsertChar` 计算光标对应的文件行和列，调用 `editorRowInsertChar`，然后移动光标。如果光标所在行不存在（比如文件末尾之后），先补足空行。

**插入换行** `editorInsertNewline` 是最复杂的编辑操作——在行中间按回车需要把当前行拆成两行：

```c
void editorInsertNewline(void) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    if (filecol == 0) {
        editorInsertRow(filerow,"",0);      // 行首回车：在上方插入空行
    } else {
        // 行中间回车：把光标之后的内容切到新行
        editorInsertRow(filerow+1, row->chars+filecol, row->size-filecol);
        row = &E.row[filerow];   // realloc 后指针可能失效，重新取
        row->chars[filecol] = '\0';
        row->size = filecol;
        editorUpdateRow(row);
    }
    // 光标移到下一行行首
}
```

注意 `editorInsertRow` 内部调用了 `realloc`，这会使 `E.row` 数组的地址可能改变，所以拆行后必须重新获取 `row` 指针。这是 C 语言里动态数组的经典陷阱。

**删除字符** `editorDelChar` 分两种情况：光标在行中间就删除前一个字符；光标在行首就把当前行合并到上一行（用 `editorRowAppendString` + `editorDelRow`）。

## 6. 文件读写

### 6.1 打开文件

```c
int editorOpen(char *filename) {
    FILE *fp;

    E.dirty = 0;
    free(E.filename);
    size_t fnlen = strlen(filename)+1;
    E.filename = malloc(fnlen);
    memcpy(E.filename,filename,fnlen);

    fp = fopen(filename, "r");
    if (!fp) {
        if (errno != ENOENT) { perror("Opening file"); exit(1); }
        return 1;    // 文件不存在时当作新建
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        // 去掉行尾的一个 \n 或 \r
        if (linelen && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
            line[--linelen] = '\0';
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;   // 读入不算"修改"
    return 0;
}
```

kilo 用 POSIX 的 `getline` 逐行读取，避免了手动管理行缓冲区大小。注意最后把 `E.dirty` 重置为 0——`editorInsertRow` 每次调用都会递增 `dirty`，但读取文件不应算作修改。

### 6.2 保存文件

```c
int editorSave(void) {
    int len;
    char *buf = editorRowsToString(&len);   // 把所有行拼成一个字符串
    int fd = open(E.filename, O_RDWR|O_CREAT, 0644);
    if (fd == -1) goto writeerr;

    if (ftruncate(fd, len) == -1) goto writeerr;  // 先截断
    if (write(fd, buf, len) != len) goto writeerr; // 再写入

    close(fd);
    free(buf);
    E.dirty = 0;
    editorSetStatusMessage("%d bytes written on disk", len);
    return 0;

writeerr:
    free(buf);
    if (fd != -1) close(fd);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
    return 1;
}
```

保存策略是 `ftruncate` + 单次 `write`，而不是先删后建。这样在写入失败时，原文件至多被截断但不会消失。`editorRowsToString` 遍历所有行，拼成 `\n` 分隔的大字符串。

## 7. 屏幕渲染

### 7.1 Append Buffer

每次刷屏需要输出大量转义序列和文本。如果逐个 `write`，屏幕会闪烁。kilo 用一个简单的 append buffer 把所有内容攒起来，最后一次 `write` 输出：

```c
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL,0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len+len);
    if (new == NULL) return;
    memcpy(new+ab->len, s, len);
    ab->b = new;
    ab->len += len;
}
```

这就是一个最小的动态字节缓冲区——和 `editorRowAppendString` 的思路一样，每次追加都 `realloc`。正文是逐字符追加的，每帧的追加次数和屏幕上的字符数差不多；在这个规模下，这样写简单直接。

### 7.2 刷屏流程

`editorRefreshScreen` 是 kilo 中较长的函数之一（约 120 行），做这几件事：

1. **隐藏光标**：`ESC[?25l`，避免渲染过程中光标闪烁。
2. **光标归位**：`ESC[H`，回到左上角开始画。
3. **逐行绘制**：对每一行：
   - 如果该行不存在（文件行数不够），画一个 `~`。
   - 如果文件为空，在屏幕 1/3 处显示欢迎信息。
   - 否则，逐字符输出，根据 `hl[]` 数组插入颜色转义序列。
4. **画状态栏**：反色显示文件名、行数和当前位置。
5. **画消息栏**：显示最近 5 秒内的状态消息。
6. **定位光标**：`ESC[行;列H`，把光标移到正确位置。光标列需要考虑 Tab 展开的偏移。
7. **显示光标**：`ESC[?25h`。
8. **一次性输出**：`write(STDOUT_FILENO, ab.b, ab.len)`。

绘制正文时，kilo 用变量 `current_color` 记录当前颜色，只在颜色实际改变时才输出 `ESC[颜色m`，减少不必要的转义序列：

```c
int color = editorSyntaxToColor(hl[j]);
if (color != current_color) {
    char buf[16];
    int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
    current_color = color;
    abAppend(&ab, buf, clen);
}
abAppend(&ab, c+j, 1);
```

### 7.3 滚动

kilo 没有单独的滚动函数。滚动是**渲染的副作用**：`E.rowoff` 和 `E.coloff` 记录当前视口的起始行列，`editorRefreshScreen` 总是从 `E.rowoff` 行开始画 `E.screenrows` 行。光标移动函数在光标超出视口边界时更新这两个偏移量，下次刷屏自然就"滚动"了。

## 8. 增量搜索

`editorFind` 实现了一个交互式的增量搜索：用户每输入一个字符，立即搜索并跳转到匹配位置。

```c
void editorFind(int fd) {
    char query[KILO_QUERY_LEN+1] = {0};
    int qlen = 0;
    int last_match = -1;  // 上次匹配所在的行，-1 表示没有
    int find_next = 0;    // 1 向下搜，-1 向上搜
    int saved_hl_line = -1;
    char *saved_hl = NULL;

#define FIND_RESTORE_HL do { \
    if (saved_hl) { \
        memcpy(E.row[saved_hl_line].hl,saved_hl, E.row[saved_hl_line].rsize); \
        free(saved_hl); \
        saved_hl = NULL; \
    } \
} while (0)

    // 保存进入搜索前的光标位置
    int saved_cx = E.cx, saved_cy = E.cy;
    int saved_coloff = E.coloff, saved_rowoff = E.rowoff;

    while(1) {
        editorSetStatusMessage(
            "Search: %s (Use ESC/Arrows/Enter)", query);
        editorRefreshScreen();

        int c = editorReadKey(fd);
        if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE) {
            if (qlen != 0) query[--qlen] = '\0';
            last_match = -1;
        } else if (c == ESC || c == ENTER) {
            if (c == ESC) {
                // ESC 取消搜索，恢复光标；Enter 则停在当前位置
                E.cx = saved_cx; E.cy = saved_cy;
                E.coloff = saved_coloff; E.rowoff = saved_rowoff;
            }
            FIND_RESTORE_HL;
            editorSetStatusMessage("");
            return;
        } else if (c == ARROW_RIGHT || c == ARROW_DOWN) {
            find_next = 1;
        } else if (c == ARROW_LEFT || c == ARROW_UP) {
            find_next = -1;
        } else if (isprint(c)) {
            if (qlen < KILO_QUERY_LEN) {
                query[qlen++] = c;
                query[qlen] = '\0';
                last_match = -1;
            }
        }
        // 搜索匹配行，高亮匹配文本...
    }
}
```

搜索的细节：

- 用 `strstr` 在每行的 `render` 中查找匹配。
- 找到匹配后，保存该行原来的高亮数据（`saved_hl`），把匹配区域标记为 `HL_MATCH`（蓝色）。离开搜索或跳到下一个匹配时恢复原高亮。
- 方向键可以在匹配之间跳转，搜索会环绕（从最后一行绕回第一行）。
- 用一个 `FIND_RESTORE_HL` 宏封装高亮恢复逻辑，避免重复代码。

## 9. 语法高亮

### 9.1 高亮数据库

kilo 用一个 `editorSyntax` 结构描述一种语言的高亮规则：

```c
struct editorSyntax {
    char **filematch;                 // 文件名匹配模式
    char **keywords;                  // 关键字列表
    char singleline_comment_start[2]; // 单行注释前缀，如 "//"
    char multiline_comment_start[3];  // 多行注释开始，如 "/*"
    char multiline_comment_end[3];    // 多行注释结束，如 "*/"
    int flags;                        // 是否高亮字符串、数字
};
```

目前只内置了 C/C++ 的规则，但结构设计得可以扩展。关键字列表有一个约定：以 `|` 结尾的关键字用第二种颜色显示（用于区分类型名和语法关键字），比如 `"int|"` 和 `"return"`。

### 9.2 高亮算法

`editorUpdateSyntax` 对一行的 `render` 字符串逐字符扫描，用几个状态变量维护上下文：

- `prev_sep`：前一个字符是否为分隔符（决定当前位置能否开始一个关键字）。
- `in_string`：是否在字符串内部（存储引号字符 `'` 或 `"`，同时兼做 bool）。
- `in_comment`：是否在多行注释内部。

扫描按优先级依次检查：单行注释 → 多行注释 → 字符串 → 不可打印字符 → 数字 → 关键字 → 普通字符。找到匹配就 `continue`，跳过后续检查。

多行注释可以跨行，一行的扫描结果会影响下一行。函数末尾处理这种跨行传播：

```c
int oc = editorRowHasOpenComment(row);
if (row->hl_oc != oc && row->idx+1 < E.numrows)
    editorUpdateSyntax(&E.row[row->idx+1]);
row->hl_oc = oc;
```

如果一行末尾的"打开注释"状态发生了变化（比如用户刚输入了 `/*`），就递归地更新下一行的高亮。这个递归会沿着行链一直传播，直到状态不再变化为止。

### 9.3 颜色映射

`editorSyntaxToColor` 把高亮类型映射为终端颜色：

```c
int editorSyntaxToColor(int hl) {
    switch(hl) {
    case HL_COMMENT:
    case HL_MLCOMMENT: return 36;  // 青色
    case HL_KEYWORD1:  return 33;  // 黄色
    case HL_KEYWORD2:  return 32;  // 绿色
    case HL_STRING:    return 35;  // 品红
    case HL_NUMBER:    return 31;  // 红色
    case HL_MATCH:     return 34;  // 蓝色
    default:           return 37;  // 白色
    }
}
```

这些是 ANSI 标准的前景色编号，用 `ESC[编号m` 输出即可改变终端文字颜色。

## 10. 光标移动与视口

`editorMoveCursor` 处理四个方向键，有几处细节：

- **左键到行首**：如果不是第一行，跳到上一行末尾（类似 nano 的行为）。
- **右键到行尾**：跳到下一行开头。
- **滚动触发**：光标已在视口边缘、还要继续向外移动时，不再移动光标，而是调整 `E.rowoff`/`E.coloff`。
- **行尾修正**：向上/下移动后，如果新行比原来短，光标列会被钳位到新行的行尾。

光标位置用两套坐标表示：**屏幕坐标** `(E.cx, E.cy)` 是光标在视口中的位置；**文件坐标** `(E.coloff + E.cx, E.rowoff + E.cy)` 是光标在文件中的绝对位置。

## 11. 值得学习的 C 技术

通读 kilo 的代码，可以看到几个 C 工程实践：

### 11.1 动态数组的 realloc 模式

kilo 到处使用 `realloc` 扩容：行数组、行内字符、append buffer。模式总是 `realloc(ptr, newsize)` → `memcpy`/`memmove`。在这个规模下足够简单高效。

### 11.2 goto 做错误处理

`enableRawMode`、`getWindowSize`、`editorSave` 都用 `goto` 跳转到统一的错误出口。这是 C 语言里替代异常的标准做法。

### 11.3 宏做局部代码复用

搜索函数里的 `FIND_RESTORE_HL` 宏用 `do { ... } while(0)` 包装，确保在 `if` 等语句里表现正常。这在 C 代码中很常见。

### 11.4 atexit 保证终端恢复

`atexit(editorAtExit)` 确保不管程序怎么退出（正常退出、`exit(1)`），终端都能回到 cooked 模式。这比在每个退出路径手动调用 `disableRawMode` 可靠得多。

### 11.5 数据与代码分离

语法高亮的**规则**（关键字表、文件匹配模式）和**算法**（状态机扫描）完全分离。要新增一种语言的高亮只需要添加一组数据，不用改算法代码。

## 12. 整体回顾

用一张图总结 kilo 的调用关系：

```
main
├── initEditor          ← 初始化全局状态 E
├── editorSelectSyntaxHighlight  ← 根据扩展名匹配语法
├── editorOpen          ← 读文件，每行调用 editorInsertRow
│   └── editorInsertRow
│       └── editorUpdateRow → editorUpdateSyntax
├── enableRawMode       ← 设置终端 raw 模式
└── while(1)
    ├── editorRefreshScreen   ← 渲染整屏
    │   ├── abAppend (VT100 转义序列 + 文本)
    │   └── write(STDOUT_FILENO, ...)
    └── editorProcessKeypress ← 处理一次按键
        ├── editorReadKey     ← 读取 + 解析转义序列
        ├── editorInsertChar / editorDelChar / editorInsertNewline
        │   └── editorRowInsertChar / editorRowDelChar / ...
        │       └── editorUpdateRow → editorUpdateSyntax
        ├── editorMoveCursor
        ├── editorSave
        │   └── editorRowsToString → open → ftruncate → write
        └── editorFind
            └── strstr 搜索 + HL_MATCH 高亮
```

## 13. 习题

### 13.1 巩固

<Exercise id="50201" :d="2" :w="2">

写一个 `rawmode.c`，把第 4.1 节 `enableRawMode` 的改动逐项验证一遍。

1. 用 `tcgetattr` 保存原设置，复制到 `raw`，按 kilo 的写法清掉 `BRKINT`、`ICRNL`、`INPCK`、`ISTRIP`、`IXON`、`OPOST`、`ECHO`、`ICANON`、`IEXTEN`、`ISIG`，置上 `CS8`，`VMIN` 设 0，`VTIME` 设 1，用 `tcsetattr` 写回。
2. 进入前后各打印一次六项：`ICANON`、`ECHO`、`ISIG`、`OPOST`、`VMIN`、`VTIME`。在 raw 模式下读一个字节并打印它的十进制值，然后恢复终端。
3. 验收：raw 期间那组是 `ICANON=0 ECHO=0 ISIG=0 OPOST=0 VMIN=0 VTIME=1`；按 `a` 打印 97，按 `Ctrl-C` 打印 3，程序不会被信号终止。

最后回答两个问题：`ISIG` 关掉之后 `Ctrl-C` 为什么不再产生信号？`OPOST` 关掉之后，程序输出的换行符在终端上还会不会回到行首？

</Exercise>

<Exercise id="50202" :d="2" :w="2">

第 3.1 节的 `editorProcessKeypress` 用 `static int quit_times` 做未保存退出的倒计数。先按下表预测每步的结果，再编译验证。

文件打开后已经插入过一个字符，`E.dirty` 为 1。此后依次按键：

| 步骤 | 按键 | 状态栏显示 | 是否退出 |
|:---|:---|:---|:---|
| 1 | `Ctrl-Q` | ? | ? |
| 2 | `Ctrl-Q` | ? | ? |
| 3 | `a` | ? | ? |
| 4 | `Ctrl-Q` | ? | ? |
| 5 | `Ctrl-Q` | ? | ? |
| 6 | `Ctrl-Q` | ? | ? |
| 7 | `Ctrl-Q` | ? | ? |

要求：

1. 把表填完，每格写出状态栏文字与其中的倒计数。
2. 把源码里的 `KILO_QUIT_TIMES` 改成 1，重新编译，用同样的按键序列跑一遍，说明表里哪些格子变了。
3. 回答：去掉 `quit_times` 前面的 `static`，行为会变成什么？这个对象为什么必须跨调用保持值？

</Exercise>

<Exercise id="50203" :d="2" :w="2">

`erow` 同时保存 `chars` 和 `render`，Tab 在 `render` 里展开为空格。实现两个函数：

```c
/* 把 s 的前 n 个字节展开到 out，返回写入的字节数；制表位按 8 列对齐。 */
int expand_tabs(const char *s, int n, char *out);

/* 光标位于 s 的第 n 个字节之后，返回它所在的屏幕列号。 */
int screen_col(const char *s, int n);
```

要求：

1. 对任意前缀，`expand_tabs` 写入的字节数必须等于 `screen_col` 返回的列号。
2. 用下表逐行打印两个返回值，与期望对照（“制表符”指一个字节的 Tab）：

| 输入 | `expand_tabs` | `screen_col` |
|:---|:---|:---|
| 空串 | 0 | 0 |
| `abc` | 3 | 3 |
| 一个制表符 | 8 | 8 |
| `a` 加制表符加 `b` | 9 | 9 |
| `abc` 加制表符加 `de` | 10 | 10 |
| `12345678` 加制表符 | 16 | 16 |
| 两个制表符 | 16 | 16 |

最后回答：`editorRefreshScreen` 定位光标时为什么不直接用 `E.cx`，而要从 `render` 重新算列号？如果制表位改成 4 列，上表哪几行的值会变？

</Exercise>

### 13.2 编程

<Exercise id="50204" :d="3" :w="3">

按第 6 节实现文件的读入与写回，写一个 `filecopy.c`：

```text
./filecopy 源文件 目标文件
```

1. 读入用 `getline` 逐行读取，每行去掉行尾的一个 `\n` 或 `\r`，存进一个动态的字符行数组。读完之后打印总行数与总字节数。
2. 写回用 `open(目标, O_RDWR|O_CREAT, 0644)`、`ftruncate`、一次 `write`，把所有行用 `\n` 连接起来写出，末尾补一个换行。不要用 `fopen("w")`。
3. 验收：准备一个 10 行的源文件，目标文件先用 20 行内容占着，复制之后 `cmp 源文件 目标文件` 无输出，`wc -l` 显示 10 行，目标文件里原来的内容一点不剩。
4. 再验证：目标文件不存在时会新建；源文件最后一行没有换行时，复制结果末尾仍然带一个换行。

最后回答：kilo 为什么选 `ftruncate` 加 `write`，而不是先 `unlink` 再重建？按 kilo 的做法，写到一半失败时磁盘上的文件处于什么状态？

</Exercise>

<Exercise id="50205" :d="3" :w="3">

第 7.1 节的 append buffer 每帧只调用一次 `write`。写一个 `frame.c` 把这件事测出来：给定 24 行文本，把整屏内容拼进一个 `struct abuf`，最后一次性写到标准输出。

1. 每行先输出 `\x1b[0K` 清行，再输出文本，最后 `\r\n`；不足 24 行的部分输出 `~`。前 8 个字符用红色（`\x1b[31m`），之后恢复 `\x1b[39m`。
2. 另写一份 `frame_bad.c`，同样的内容但每个字符调用一次 `write`。
3. 用 strace 数 `write` 的次数：

```console
$ strace -e trace=write -c ./frame < t.txt > /dev/null
$ strace -e trace=write -c ./frame_bad < t.txt > /dev/null
```

4. 验收：`frame` 里拼帧的那一次输出就是一次 `write`（其余几次数的是启动与退出时别的写操作），`frame_bad` 的次数是文本长度的量级；把两者重定向到文件后 `cmp` 输出相同。

最后回答：为什么逐字符 `write` 会让屏幕闪烁，而攒进缓冲区后一次性写出不会？

</Exercise>

<Exercise id="50206" :d="3" :w="3">

第 8 节的增量搜索靠 `strstr` 和环绕跳转。先不碰终端，把搜索核心单独写出来：

```c
/* 从 start 行开始（含），direction 为 1 向下、-1 向上，查找包含 query 的行。
 * 找到返回行号，找不到返回 -1；搜索绕回，最多检查 lines 行。 */
int find_line(char **lines, int count, int start, const char *query, int direction);
```

1. `lines` 是每行的 `render` 文本，用下面的数据测试：

```c
char *doc[] = {"int main(void) {", "    printf(\"hi\");", "    return 0;", "}"};
```

2. 依次验证这几组调用并写出期望输出：从第 0 行向下找 `return`；从第 2 行向下找 `int`（应绕回第 0 行）；从第 0 行向上找 `printf`（应绕回第 2 行）；找不存在的 `while`；query 为空串。
3. 增加一个 `find_all`，打印所有匹配行的行号与列号（列号用 `strstr` 的返回值减去行首）。
4. 验收：程序的输出与你手算的表逐行一致。

最后回答：kilo 的 `editorFind` 为什么在每次匹配后先保存 `E.row[e].hl`，离开搜索时再恢复？如果直接改 `hl` 不恢复，屏幕会留下什么？

</Exercise>

<Exercise id="50207" :d="4" :w="3">

第 9.1 节说语法高亮的规则是数据，新增一种语言只改数据。给 kilo 加上 Python 的规则：

1. 在 `editorSyntax` 数组里新增一项，文件匹配写 `"*.py"`，关键字里 `def`、`class`、`if`、`else`、`for` 作第一类颜色，`str`、`int`、`list`、`dict` 末尾加 `|` 作第二类颜色，单行注释前缀 `#`，多行注释留空，`flags` 打开字符串与数字高亮。
2. 写一个 `test.py` 覆盖三件事：`#` 开头的注释整行变色；字符串里的 `def` 不变色；`int` 与 `if` 显示成两种颜色。
3. 用 `kilo_drive -o raw.out` 保存原始输出，在 `raw.out` 里找出 `\x1b[33m`（第一类关键字）与 `\x1b[32m`（第二类关键字）各出现的位置，验证 `int` 走的是第二种颜色。
4. 验收：同一份 `test.py` 在改动前显示为纯白，改动后出现青、黄、绿、品红至少四种颜色；`make test` 仍然全过。

最后回答：为什么多行注释字段留空串就能让 `editorUpdateSyntax` 跳过跨行传播？如果 Python 的关键字表里漏掉某个词，代码需要改几处？

</Exercise>

<Exercise id="50208" :d="4" :w="3">

第 5.3 节的拆行代码在 `editorInsertRow` 之后重新取了 `row` 指针。用一个小程序把这个陷阱复现出来：

```c
struct row {
    int size;
    char *chars;
};
```

1. 写 `append_row(struct row **rows, int *n, const char *s)`，内部 `realloc` 扩容并复制字符串。
2. 写 `split_row(struct row **rows, int *n, int at, int col)`：取出 `rows[at]` 的指针，调用 `append_row` 插入新行，然后**不重新取指针**，直接写 `r->chars[col] = '\0'; r->size = col;`。
3. 用 `-fsanitize=address -g` 编译，构造一个足够长的行（比如 100 个字符）让 `realloc` 换块，观察 ASan 的 `heap-use-after-free` 报告，记录分配与释放的调用点。
4. 修正：在插入后重新取 `r = &(*rows)[at]`，再跑一次，报告消失，程序输出的两行内容正确。
5. 验收：两次运行的输出都贴出来，说明第一次的哪一行写入落在了已释放的内存上。

最后回答：`erow` 结构里有没有别的指针会因为 `E.row` 的 `realloc` 失效？`editorRefreshScreen` 里 `r = &E.row[filerow]` 为什么是安全的？

</Exercise>

<Exercise id="50209" :d="4" :w="4">

第 3.2 节靠 VTIME 超时区分单独的 Esc 和转义序列，第 4.2 节在 `ioctl` 失败时用 `ESC[999C` 加 `ESC[6n` 兜底。把这两件事各写成一个小程序。

1. 写 `keycode.c`，用 `editorReadKey` 的同款逻辑，把下列输入翻译成逻辑按键并打印：`a`、`ESC [ A`、`ESC [ 5 ~`、单独的 `ESC`（分三次写入，间隔 50 毫秒）、`ESC` 后紧跟 `x`。最后一种的期望是什么？
2. 写 `winsize.c`，先试 `ioctl(1, TIOCGWINSZ, &ws)`，失败或 `ws_col == 0` 时走兜底：记下当前光标位置（发送 `ESC[6n` 并从标准输入读回 `ESC[行;列R`），发送 `ESC[999C ESC[999B`，再查一次光标位置，最后用 `ESC[行;列H` 恢复。两种路径打印的尺寸都要和 `stty size` 的结果一致。
3. 用 `script` 或 `kilo_drive` 提供一个伪终端来运行 `winsize`，把窗口设为 24 行 80 列，验证兜底分支得到 `24 80`。在普通重定向到文件的场景下运行，说明 `ioctl` 分支会失败在哪里。
4. 验收：`keycode` 的五组输出都与预期一致，其中「`ESC` 后紧跟 `x`」的解释要写清楚，`winsize` 的两条路径都要有输出。

最后回答：`getWindowSize` 在兜底分支里为什么要先保存光标位置？如果不恢复，下一次刷屏会发生什么？

</Exercise>

### 13.3 综合与挑战

<Exercise id="50210" :d="5" :w="5">

写一个最小的终端编辑器 `tiny.c`，把第 3、4、5、7 节的主线走通，并与 kilo 的对应部分对照：

1. 进入 raw 模式（`ICANON`、`ECHO`、`ISIG` 关闭，`VMIN=0`、`VTIME=1`），用 `atexit` 注册恢复函数。
2. 文档模型用动态行数组，每行保存 `chars` 与 `size`，支持在光标处插入字符、退格、回车拆行。
3. 渲染用 append buffer，每帧一次 `write`：正文、状态栏（显示行列号与文件名）、光标定位。光标列要按 Tab 展开计算。
4. 支持方向键移动，光标到视口边界时调整 `offset`，而不是继续移动光标。
5. 用 `code/项目/kilo/kilo_drive.c` 驱动它：

```console
$ gcc -std=c11 -Wall -Wextra -o tiny tiny.c
$ cat > tiny.keys <<'EOF'
keys hello
keys <left><left>
keys XY
sleep 200
keys <C-q><C-q>
EOF
$ ./kilo_drive -l tiny.last tiny.keys -- ./tiny
$ cat tiny.last
```

6. 验收：标准输出末帧第一行是 `helXYlo`，光标停在 `Y` 之后；按 Esc 能单独识别；程序正常退出后终端设置复原。

最后回答：`tiny` 里哪一部分与 kilo 的 `editorInsertNewline` 对应？为什么拆行时也要重新取行指针？`tiny` 少了 kilo 的哪一层，导致方向键到达视口边缘时行为不同？

</Exercise>
