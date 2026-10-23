/* fail_values.c：不同函数失败时返回的句柄值，以及失败值的两种误用。 */
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    char buf[8];
    DWORD got = 0;
    BOOL read;
    DWORD read_error;
    DWORD wait;

    HANDLE file = CreateFileW(L"no_such_file_mdr.txt", GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, 0, NULL);
    printf("CreateFileW (missing file)   -> %p, error %lu\n", (void *)file, GetLastError());

    HANDLE event = OpenEventW(EVENT_ALL_ACCESS, FALSE, L"Local\\MdrTutorial.NoSuchEvent");
    printf("OpenEventW (missing event)   -> %p, error %lu\n", (void *)event, GetLastError());

    SetLastError(0);
    event = CreateEventW(NULL, FALSE, FALSE, L"bad\\name");
    printf("CreateEventW (bad name)      -> %p, error %lu\n", (void *)event, GetLastError());

    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(L"no_such_dir_mdr\\*", &data);
    printf("FindFirstFileW (missing dir) -> %p, error %lu\n", (void *)find, GetLastError());
    if (find != INVALID_HANDLE_VALUE) {
        FindClose(find);
    }

    HANDLE pipe = CreateNamedPipeW(L"not_a_pipe_name", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 0, 0,
                                   0, NULL);
    printf("CreateNamedPipeW (bad name)  -> %p, error %lu\n", (void *)pipe, GetLastError());
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
    }

    SetLastError(0);
    event = CreateEventW(NULL, FALSE, FALSE, NULL);
    printf("CreateEventW (success)       -> %p, last error %lu\n", (void *)event,
           GetLastError());
    CloseHandle(event);

    printf("INVALID_HANDLE_VALUE as an integer: %lld\n", (long long)(intptr_t)INVALID_HANDLE_VALUE);
    printf("INVALID_HANDLE_VALUE == GetCurrentProcess(): %d\n",
           INVALID_HANDLE_VALUE == GetCurrentProcess());

    /* 失败值看起来像一个正常的句柄。 */
    read = ReadFile(file, buf, sizeof buf, &got, NULL);
    read_error = GetLastError();
    printf("ReadFile on it = %d, error %lu\n", read, read_error);
    wait = WaitForSingleObject(file, 200);
    printf("WaitForSingleObject on it with a 200 ms timeout -> %lu\n", (unsigned long)wait);
    return 0;
}