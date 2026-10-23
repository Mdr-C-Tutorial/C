/* macrochk.c：看 UNICODE 宏决定 CreateFile 指向哪个版本。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
#ifdef UNICODE
    printf("UNICODE defined: CreateFile is CreateFileW\n");
#else
    printf("UNICODE undefined: CreateFile is CreateFileA\n");
#endif
#ifdef _UNICODE
    printf("_UNICODE defined\n");
#endif
    printf("sizeof(TCHAR) = %d\n", (int)sizeof(TCHAR));
    return 0;
}