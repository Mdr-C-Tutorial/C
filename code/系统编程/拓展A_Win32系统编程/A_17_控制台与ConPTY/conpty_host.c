/* conpty_host.c —— 最小的伪控制台宿主。
 *
 * 运行 cmd.exe /c echo hello，把子进程输出的原始虚拟终端字节流打印出来。
 *
 * 构建（CreatePseudoConsole 需要 Windows 10 1809 的头文件声明）：
 *   gcc -std=c17 -Wall -Wextra -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006 \
 *       conpty_host.c -o conpty_host.exe
 * 运行：conpty_host.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

/* 输出管道由单独的线程持续读走；主线程负责启动子进程和收尾。
 * 两个方向都在同一个线程里同步读写，一边缓冲区写满就会互相等待。 */
static DWORD WINAPI drain(LPVOID arg) {
    HANDLE src = (HANDLE)arg;
    char buf[4096];
    DWORD n;
    while (ReadFile(src, buf, sizeof buf, &n, NULL) && n > 0) {
        for (DWORD i = 0; i < n; i++) {
            unsigned char c = (unsigned char)buf[i];
            if (c == 0x1b) {
                fputs("\\e", stdout);
            } else if (c == '\r') {
                fputs("\\r", stdout);
            } else if (c == '\n') {
                fputs("\\n\n", stdout);
            } else if (c == 0x07) {
                fputs("\\a", stdout);
            } else if (c < 0x20 || c == 0x7f) {
                printf("\\x%02X", c);
            } else {
                putchar(c);
            }
        }
        fflush(stdout);
    }
    return 0;
}

int main(void) {
    HANDLE in_read, in_write, out_read, out_write;
    if (!CreatePipe(&in_read, &in_write, NULL, 0) || !CreatePipe(&out_read, &out_write, NULL, 0)) {
        printf("CreatePipe failed: %lu\n", GetLastError());
        return 1;
    }

    /* 输入给读端，输出给写端：伪控制台的另一侧由它自己持有。 */
    HPCON hpc;
    COORD size = {.X = 80, .Y = 25};
    HRESULT hr = CreatePseudoConsole(size, in_read, out_write, 0, &hpc);
    if (FAILED(hr)) {
        printf("CreatePseudoConsole failed: 0x%08lX\n", (unsigned long)hr);
        return 1;
    }
    /* 伪控制台已经复制了这两个句柄，宿主这一份必须关掉，
     * 否则会话关闭时读端看不到管道断开。 */
    CloseHandle(in_read);
    CloseHandle(out_write);

    HANDLE reader = CreateThread(NULL, 0, drain, out_read, 0, NULL);

    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytes);
    STARTUPINFOEXW si = {.StartupInfo.cb = sizeof si, .StartupInfo.dwFlags = STARTF_USESTDHANDLES};
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(bytes);
    if (si.lpAttributeList == NULL || !InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes) ||
        !UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof hpc, NULL,
                                   NULL)) {
        printf("attribute list failed: %lu\n", GetLastError());
        return 1;
    }

    wchar_t cmd[] = L"cmd.exe /c echo hello";
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi)) {
        printf("CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }

    WaitForSingleObject(pi.hProcess, 10000);
    /* 关掉伪控制台之后，输出管道读到 0 字节，读线程自己结束。 */
    ClosePseudoConsole(hpc);
    WaitForSingleObject(reader, 5000);
    printf("\n");

    CloseHandle(reader);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(in_write);
    CloseHandle(out_read);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    free(si.lpAttributeList);
    return 0;
}