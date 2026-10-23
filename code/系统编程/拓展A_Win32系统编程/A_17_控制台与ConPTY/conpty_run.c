/* conpty_run.c —— 用伪控制台（ConPTY）运行一个控制台程序并记录它的屏幕输出。
 *
 * 用法：
 *   conpty_run [-raw] [-in 文本] [-delay 毫秒] [-timeout 毫秒] [-cols N] [-rows N] -- 命令行
 *
 *   -raw       原样打印虚拟终端字节流（ESC 显示为 \e，CR 显示为 \r，LF 显示为 \n 并换行）；
 *              默认打印去掉控制序列后的文本（CR LF 合并为换行）。
 *   -in 文本   子进程启动 delay 毫秒后写入其输入，支持 \r \n \e \\ 和 \xHH，
 *              例如 "abc\r" 表示输入 abc 后回车，"\x03" 表示 Ctrl+C。
 *   -delay     写入输入前等待的毫秒数，默认 500。
 *   -timeout   等待子进程结束的毫秒数，默认 10000，超时则结束子进程。
 *
 * 构建：
 *   gcc -std=c17 -Wall -Wextra -municode -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006 \
 *       conpty_run.c -o conpty_run.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int raw_mode = 0;

/* 把输出管道里的字节写成可读形式。文本模式下去掉 CSI 序列，只留光标右移的效果。 */
static DWORD WINAPI drain_output(LPVOID arg) {
    HANDLE src = (HANDLE)arg;
    unsigned char buf[4096];
    DWORD n;
    int state = 0; /* 0 普通，1 收到 ESC，2 CSI 中，3 OSC 中，4 OSC 中收到 ESC，5 跳过一个字符 */
    char param[32];
    int plen = 0;

    while (ReadFile(src, buf, sizeof buf, &n, NULL) && n > 0) {
        for (DWORD i = 0; i < n; i++) {
            unsigned char c = buf[i];
            if (raw_mode) {
                if (c == 0x1b) {
                    fputs("\\e", stdout);
                } else if (c == '\r') {
                    fputs("\\r", stdout);
                } else if (c == '\n') {
                    fputs("\\n\n", stdout);
                } else if (c == 0x07) {
                    fputs("\\a", stdout);
                } else {
                    putchar(c);
                }
                continue;
            }

            switch (state) {
            case 0:
                if (c == 0x1b) {
                    state = 1;
                } else if (c == '\n') {
                    putchar('\n');
                } else if (c != '\r' && c != 0x07) {
                    putchar(c);
                }
                break;
            case 1:
                if (c == '[') {
                    state = 2;
                    plen = 0;
                } else if (c == ']') {
                    state = 3;
                } else if (c == '(' || c == ')') {
                    state = 5;
                } else {
                    state = 0;
                }
                break;
            case 2:
                if (c >= 0x40 && c <= 0x7e) {
                    param[plen] = '\0';
                    if (c == 'C') {
                        int count = plen > 0 ? atoi(param) : 1;
                        for (int k = 0; k < count; k++) {
                            putchar(' ');
                        }
                    }
                    state = 0;
                } else if (plen < (int)sizeof param - 1) {
                    param[plen++] = (char)c;
                }
                break;
            case 3:
                if (c == 0x07) {
                    state = 0;
                } else if (c == 0x1b) {
                    state = 4;
                }
                break;
            case 4:
                state = (c == '\\') ? 0 : 3;
                break;
            case 5:
                state = 0;
                break;
            }
        }
        fflush(stdout);
    }
    return 0;
}

/* 把命令行里的转义写法解码成字节。 */
static size_t decode_escapes(const wchar_t *src, char *dst, size_t cap) {
    char utf8[1024];
    int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, utf8, (int)sizeof utf8, NULL, NULL);
    size_t out = 0;

    for (int i = 0; i < len - 1 && out < cap; i++) {
        char c = utf8[i];
        if (c == '\\' && i + 1 < len - 1) {
            char e = utf8[++i];
            if (e == 'r') {
                dst[out++] = '\r';
            } else if (e == 'n') {
                dst[out++] = '\n';
            } else if (e == 'e') {
                dst[out++] = 0x1b;
            } else if (e == 'x' && i + 2 < len - 1) {
                char hex[3] = {utf8[i + 1], utf8[i + 2], '\0'};
                dst[out++] = (char)strtol(hex, NULL, 16);
                i += 2;
            } else {
                dst[out++] = e;
            }
        } else {
            dst[out++] = c;
        }
    }
    return out;
}

int wmain(int argc, wchar_t **argv) {
    const wchar_t *input = NULL;
    DWORD delay = 500;
    DWORD timeout = 10000;
    SHORT cols = 80;
    SHORT rows = 25;
    int i = 1;

    for (; i < argc; i++) {
        if (wcscmp(argv[i], L"-raw") == 0) {
            raw_mode = 1;
        } else if (wcscmp(argv[i], L"-in") == 0 && i + 1 < argc) {
            input = argv[++i];
        } else if (wcscmp(argv[i], L"-delay") == 0 && i + 1 < argc) {
            delay = (DWORD)wcstoul(argv[++i], NULL, 10);
        } else if (wcscmp(argv[i], L"-timeout") == 0 && i + 1 < argc) {
            timeout = (DWORD)wcstoul(argv[++i], NULL, 10);
        } else if (wcscmp(argv[i], L"-cols") == 0 && i + 1 < argc) {
            cols = (SHORT)wcstol(argv[++i], NULL, 10);
        } else if (wcscmp(argv[i], L"-rows") == 0 && i + 1 < argc) {
            rows = (SHORT)wcstol(argv[++i], NULL, 10);
        } else if (wcscmp(argv[i], L"--") == 0) {
            i++;
            break;
        } else {
            break;
        }
    }
    if (i >= argc) {
        fputs("usage: conpty_run [-raw] [-in text] [-delay ms] [-timeout ms] -- command line\n", stderr);
        return 2;
    }

    /* 从「忽略 Ctrl+C」的父进程继承来的标志会被子进程继承，先清除。 */
    SetConsoleCtrlHandler(NULL, FALSE);

    /* 剩余参数拼成命令行；含空格的参数加引号。 */
    size_t total = 1;
    for (int k = i; k < argc; k++) {
        total += wcslen(argv[k]) + 3;
    }
    wchar_t *cmd = malloc(total * sizeof *cmd);
    if (cmd == NULL) {
        return 1;
    }
    cmd[0] = L'\0';
    for (int k = i; k < argc; k++) {
        if (k > i) {
            wcscat(cmd, L" ");
        }
        int quote = (k > i) && wcschr(argv[k], L' ') != NULL;
        if (quote) {
            wcscat(cmd, L"\"");
        }
        wcscat(cmd, argv[k]);
        if (quote) {
            wcscat(cmd, L"\"");
        }
    }

    HANDLE in_read, in_write, out_read, out_write;
    if (!CreatePipe(&in_read, &in_write, NULL, 0) || !CreatePipe(&out_read, &out_write, NULL, 0)) {
        fprintf(stderr, "CreatePipe failed: %lu\n", GetLastError());
        return 1;
    }

    HPCON pc;
    COORD size = {.X = cols, .Y = rows};
    HRESULT hr = CreatePseudoConsole(size, in_read, out_write, 0, &pc);
    if (FAILED(hr)) {
        fprintf(stderr, "CreatePseudoConsole failed: 0x%08lX\n", (unsigned long)hr);
        return 1;
    }
    CloseHandle(in_read);
    CloseHandle(out_write);

    HANDLE reader = CreateThread(NULL, 0, drain_output, out_read, 0, NULL);

    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytes);
    /* STARTF_USESTDHANDLES 且三个标准句柄为 NULL：防止子进程继承本程序被重定向的标准句柄。 */
    STARTUPINFOEXW si = {.StartupInfo.cb = sizeof si, .StartupInfo.dwFlags = STARTF_USESTDHANDLES};
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(bytes);
    if (si.lpAttributeList == NULL || !InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes) ||
        !UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pc, sizeof pc, NULL,
                                   NULL)) {
        fprintf(stderr, "attribute list failed: %lu\n", GetLastError());
        return 1;
    }

    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo,
                        &pi)) {
        fprintf(stderr, "CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }

    if (input != NULL) {
        Sleep(delay);
        char bytes_in[1024];
        size_t len = decode_escapes(input, bytes_in, sizeof bytes_in);
        DWORD written;
        WriteFile(in_write, bytes_in, (DWORD)len, &written, NULL);
    }

    DWORD code = 0;
    if (WaitForSingleObject(pi.hProcess, timeout) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 0xDEAD);
        WaitForSingleObject(pi.hProcess, INFINITE);
        fputs("\n[timeout: child terminated]\n", stdout);
    }
    GetExitCodeProcess(pi.hProcess, &code);
    ClosePseudoConsole(pc);
    WaitForSingleObject(reader, 5000);
    printf("\n[exit code %lu (0x%08lX)]\n", (unsigned long)code, (unsigned long)code);

    CloseHandle(reader);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(in_write);
    CloseHandle(out_read);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    free(si.lpAttributeList);
    free(cmd);
    return 0;
}