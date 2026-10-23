/* 访问掩码的组成：对象专用权限、标准权限、通用权限位，以及 MapGenericMask 的映射结果。 */
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    static const struct {
        const char *name;
        DWORD value;
    } bits[] = {
        {"FILE_READ_DATA", FILE_READ_DATA},
        {"FILE_WRITE_DATA", FILE_WRITE_DATA},
        {"FILE_APPEND_DATA", FILE_APPEND_DATA},
        {"FILE_READ_EA", FILE_READ_EA},
        {"FILE_WRITE_EA", FILE_WRITE_EA},
        {"FILE_EXECUTE", FILE_EXECUTE},
        {"FILE_DELETE_CHILD", FILE_DELETE_CHILD},
        {"FILE_READ_ATTRIBUTES", FILE_READ_ATTRIBUTES},
        {"FILE_WRITE_ATTRIBUTES", FILE_WRITE_ATTRIBUTES},
        {"DELETE", DELETE},
        {"READ_CONTROL", READ_CONTROL},
        {"WRITE_DAC", WRITE_DAC},
        {"WRITE_OWNER", WRITE_OWNER},
        {"SYNCHRONIZE", SYNCHRONIZE},
        {"GENERIC_READ", GENERIC_READ},
        {"GENERIC_WRITE", GENERIC_WRITE},
        {"GENERIC_EXECUTE", GENERIC_EXECUTE},
        {"GENERIC_ALL", GENERIC_ALL},
        {"FILE_GENERIC_READ", FILE_GENERIC_READ},
        {"FILE_GENERIC_WRITE", FILE_GENERIC_WRITE},
        {"FILE_GENERIC_EXECUTE", FILE_GENERIC_EXECUTE},
        {"FILE_ALL_ACCESS", FILE_ALL_ACCESS},
    };
    GENERIC_MAPPING map = {FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE,
                           FILE_ALL_ACCESS};
    DWORD mask;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    for (size_t i = 0; i < sizeof bits / sizeof bits[0]; i++) {
        printf("%-24s 0x%08lX\n", bits[i].name, (unsigned long)bits[i].value);
    }

    printf("\nMapGenericMask:\n");
    mask = GENERIC_READ;
    MapGenericMask(&mask, &map);
    printf("GENERIC_READ          -> 0x%08lX\n", (unsigned long)mask);
    mask = GENERIC_WRITE;
    MapGenericMask(&mask, &map);
    printf("GENERIC_WRITE         -> 0x%08lX\n", (unsigned long)mask);
    mask = GENERIC_READ | GENERIC_WRITE;
    MapGenericMask(&mask, &map);
    printf("GENERIC_READ|WRITE    -> 0x%08lX\n", (unsigned long)mask);
    mask = GENERIC_ALL;
    MapGenericMask(&mask, &map);
    printf("GENERIC_ALL           -> 0x%08lX\n", (unsigned long)mask);
    return 0;
}