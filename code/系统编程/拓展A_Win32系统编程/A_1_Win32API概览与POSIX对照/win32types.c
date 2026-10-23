/* win32types.c：Win32 类型在 x64 上的大小。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    printf("BOOL          %llu\n", (unsigned long long)sizeof(BOOL));
    printf("DWORD         %llu\n", (unsigned long long)sizeof(DWORD));
    printf("LONG          %llu\n", (unsigned long long)sizeof(LONG));
    printf("WCHAR         %llu\n", (unsigned long long)sizeof(WCHAR));
    printf("HANDLE        %llu\n", (unsigned long long)sizeof(HANDLE));
    printf("SIZE_T        %llu\n", (unsigned long long)sizeof(SIZE_T));
    printf("LARGE_INTEGER %llu\n", (unsigned long long)sizeof(LARGE_INTEGER));
    printf("long          %llu\n", (unsigned long long)sizeof(long));
    printf("void *        %llu\n", (unsigned long long)sizeof(void *));
    return 0;
}