# 2.1 源码阅读：kilo

kilo 是 antirez（Redis 的作者）写的一个终端文本编辑器，整个项目只有一个 C 文件，不到 1000 行有效代码（以 cloc 计）。它不依赖 ncurses，直接向终端发送 VT100 转义序列完成所有渲染，是学习「C 如何与终端交互」的理想材料。

本文从**目的到实现**自顶向下地拆解 kilo 的源码。读完后你会理解：一个终端文本编辑器到底需要解决哪些问题，以及 kilo 用了哪些 C 技术来解决它们。

> 源码仓库：<https://github.com/antirez/kilo>，BSD-2-Clause 许可。本文基于截至 2016 年的最新版本。

## 1. 一个终端编辑器需要做什么

在终端里运行 `vim` 或 `nano` 时，你看到的是一整屏实时刷新的文字。但终端本身只是一个逐行输入、逐行输出的设备——它并不天然支持"光标移到第 3 行第 7 列"这种操作。所以一个终端编辑器至少需要解决五个问题：

1. **接管终端**：关闭终端的"行缓冲"和"回显"，让程序能逐字节读取用户的每次按键。
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

这是 C 语言里常见的"单实例全局状态"模式。整个程序通过 `E.cx`、`E.row` 等成员访问一切，不需要在函数间传递上下文指针。

### 2.2 主循环

`main` 函数只有 10 行，把全部细节委托出去：

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

核心就是一个无限循环：**画屏幕 → 等按键 → 处理按键 → 再画屏幕**。这是几乎所有交互式终端程序的骨架。

### 2.3 源码分区

kilo.c 用注释横线把代码分成清晰的六个区域，从底层到高层依次是：

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

`editorProcessKeypress` 读取一次按键，然后用 `switch` 分发：

```c
void editorProcessKeypress(int fd) {
    static int quit_times = KILO_QUIT_TIMES;
    int c = editorReadKey(fd);

    switch(c) {
    case ENTER:       editorInsertNewline();  break;
    case CTRL_Q:      /* 退出，带未保存保护 */ break;
    case CTRL_S:      editorSave();           break;
    case CTRL_F:      editorFind(fd);         break;
    case BACKSPACE:
    case CTRL_H:
    case DEL_KEY:     editorDelChar();        break;
    case ARROW_UP: case ARROW_DOWN:
    case ARROW_LEFT: case ARROW_RIGHT:
        editorMoveCursor(c);                  break;
    default:          editorInsertChar(c);    break;
    }
    quit_times = KILO_QUIT_TIMES;
}
```

值得注意的设计：

- **未保存退出保护**：`Ctrl-Q` 在文件 dirty 时不会直接退出，而是要求连按 3 次。用一个 `static` 局部变量 `quit_times` 做倒计数，任何其他按键都会重置它。
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
    // 最少读 0 字节（非阻塞），超时 100ms
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
        // 备用方案：把光标移到右下角，再查询位置
        if (write(ofd,"\x1b[999C\x1b[999B",12) != 12) goto failed;
        return getCursorPosition(ifd,ofd,rows,cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}
```

备用方案的原理：`ESC[999C` 把光标向右移 999 列（会停在右边界），`ESC[999B` 向下移 999 行（停在底边界），然后用 `ESC[6n` 查询光标当前位置——这个位置就是终端的尺寸。

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

为什么需要两份内容（`chars` 和 `render`）？因为 Tab 字符在文件里是一个字节，但在屏幕上占多列（kilo 按 8 列对齐）。`render` 是把 Tab 展开成空格后的版本，`hl` 数组和 `render` 等长，记录每个字符应该用什么颜色渲染。

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
    E.filename = malloc(strlen(filename)+1);
    memcpy(E.filename, filename, strlen(filename)+1);

    fp = fopen(filename, "r");
    if (!fp) {
        if (errno != ENOENT) { perror("Opening file"); exit(1); }
        return 1;    // 文件不存在时当作新建
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        // 去掉行尾的 \n 和 \r
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

这就是一个最小的动态字节缓冲区——和 `editorRowAppendString` 的思路一样，每次追加都 `realloc`。在这个场景下每帧只追加几十次，简单直接。

### 7.2 刷屏流程

`editorRefreshScreen` 是 kilo 最长的函数（约 120 行），做这几件事：

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

颜色渲染部分的逻辑值得关注——kilo 维护一个 `current_color` 变量，只在颜色实际改变时才输出 `ESC[颜色m`，减少不必要的转义序列：

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
    int last_match = -1;
    int find_next = 0;    // 1 向下搜，-1 向上搜
    int saved_hl_line = -1;
    char *saved_hl = NULL;

    // 保存进入搜索前的光标位置
    int saved_cx = E.cx, saved_cy = E.cy;
    int saved_coloff = E.coloff, saved_rowoff = E.rowoff;

    while(1) {
        editorSetStatusMessage("Search: %s (Use ESC/Arrows/Enter)", query);
        editorRefreshScreen();

        int c = editorReadKey(fd);
        if (c == ESC) {
            // ESC 取消搜索，恢复光标
            E.cx = saved_cx; E.cy = saved_cy;
            E.coloff = saved_coloff; E.rowoff = saved_rowoff;
            /* 恢复高亮... */
            return;
        } else if (c == ENTER) {
            return;  // Enter 确认搜索，停在当前位置
        } else if (c == ARROW_RIGHT || c == ARROW_DOWN) {
            find_next = 1;
        } else if (c == ARROW_LEFT || c == ARROW_UP) {
            find_next = -1;
        } else if (isprint(c)) {
            query[qlen++] = c;
            query[qlen] = '\0';
            last_match = -1;
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

目前只内置了 C/C++ 的规则，但结构设计得可以扩展。关键字列表有一个巧妙的约定：以 `|` 结尾的关键字用第二种颜色显示（用于区分类型名和语法关键字），比如 `"int|"` 和 `"return"`。

### 9.2 高亮算法

`editorUpdateSyntax` 对一行的 `render` 字符串逐字符扫描，用几个状态变量维护上下文：

- `prev_sep`：前一个字符是否为分隔符（决定当前位置能否开始一个关键字）。
- `in_string`：是否在字符串内部（存储引号字符 `'` 或 `"`，同时兼做 bool）。
- `in_comment`：是否在多行注释内部。

扫描按优先级依次检查：单行注释 → 多行注释 → 字符串 → 不可打印字符 → 数字 → 关键字 → 普通字符。找到匹配就 `continue`，跳过后续检查。

最值得注意的是**多行注释的跨行传播**：

```c
int oc = editorRowHasOpenComment(row);
if (row->hl_oc != oc && row->idx+1 < E.numrows)
    editorUpdateSyntax(&E.row[row->idx+1]);
row->hl_oc = oc;
```

如果一行末尾的"打开注释"状态发生了变化（比如用户刚输入了 `/*`），就递归地更新下一行的高亮。这个递归会沿着行链一直传播，直到状态不再变化为止。

### 9.3 颜色映射

高亮类型到终端颜色的映射很简单：

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

`editorMoveCursor` 处理四个方向键。逻辑虽然直白，但有几个边界细节：

- **左键到行首**：如果不是第一行，跳到上一行末尾（类似 nano 的行为）。
- **右键到行尾**：跳到下一行开头。
- **滚动触发**：如果光标移出了视口（`E.cy < 0` 或 `E.cy >= E.screenrows`），调整 `E.rowoff`/`E.coloff`。
- **行尾修正**：向上/下移动后，如果新行比原来短，光标列会被钳位到新行的行尾。

光标位置用两套坐标表示：**屏幕坐标** `(E.cx, E.cy)` 是光标在视口中的位置；**文件坐标** `(E.coloff + E.cx, E.rowoff + E.cy)` 是光标在文件中的绝对位置。这种分离让滚动逻辑变得自然。

## 11. 值得学习的 C 技术

通读 kilo 的代码，可以看到几个 C 工程实践：

### 动态数组的 realloc 模式

kilo 到处使用 `realloc` 扩容：行数组、行内字符、append buffer。模式总是 `realloc(ptr, newsize)` → `memcpy`/`memmove`。在这个规模下足够简单高效。

### goto 做错误处理

`enableRawMode`、`getWindowSize`、`editorSave` 都用 `goto` 跳转到统一的错误出口。这是 C 语言里替代异常的标准做法。

### 宏做局部代码复用

搜索函数里的 `FIND_RESTORE_HL` 宏用 `do { ... } while(0)` 包装，确保在 `if` 等语句里表现正常。这在 C 代码中很常见。

### atexit 保证终端恢复

`atexit(editorAtExit)` 确保不管程序怎么退出（正常退出、`exit(1)`），终端都能回到 cooked 模式。这比在每个退出路径手动调用 `disableRawMode` 可靠得多。

### 数据与代码分离

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

1300 行代码，实现了文件编辑、语法高亮、增量搜索、滚动、状态栏——没有任何外部依赖。kilo 的价值不在于功能完善，而在于它用最少的代码演示了终端编辑器背后的全部核心问题。
