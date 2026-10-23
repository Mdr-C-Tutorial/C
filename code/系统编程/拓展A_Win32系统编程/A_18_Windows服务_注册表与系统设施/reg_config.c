/* 用注册表保存一小段配置：写 HKCU\Software\MdrTutorial\Echo，读回、枚举，最后删除。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o reg_config.exe reg_config.c
 * 用法：reg_config.exe [keep]   keep 表示保留测试项，便于用 reg query 查看
 */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define CONFIG_KEY L"Software\\MdrTutorial\\Echo"

static void print_error(const wchar_t *what, LSTATUS st) {
    WCHAR *msg = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, (DWORD)st, 0, (LPWSTR)&msg, 0, NULL);
    wprintf(L"%ls: %ld: %ls", what, (long)st, msg != NULL ? msg : L"\n");
    LocalFree(msg);
}

/* 字符串长度按字节计，包含结尾空字符。 */
static LSTATUS write_config(HKEY key) {
    const wchar_t greeting[] = L"hello";
    const wchar_t data_dir[] = L"%LOCALAPPDATA%\\MdrTutorial\\Echo";
    DWORD port = 23456;
    LSTATUS st;

    st = RegSetValueExW(key, L"Port", 0, REG_DWORD, (const BYTE *)&port, sizeof port);
    if (st != ERROR_SUCCESS) {
        return st;
    }
    st = RegSetValueExW(key, L"Greeting", 0, REG_SZ, (const BYTE *)greeting,
                        (DWORD)sizeof greeting);
    if (st != ERROR_SUCCESS) {
        return st;
    }
    return RegSetValueExW(key, L"DataDir", 0, REG_EXPAND_SZ, (const BYTE *)data_dir,
                          (DWORD)sizeof data_dir);
}

static void read_config(HKEY key) {
    DWORD port = 0;
    DWORD size = sizeof port;
    DWORD type = 0;
    LSTATUS st = RegGetValueW(key, NULL, L"Port", RRF_RT_REG_DWORD, &type, &port, &size);

    if (st == ERROR_SUCCESS) {
        wprintf(L"Port     = %lu (type %lu)\n", (unsigned long)port, (unsigned long)type);
    } else {
        print_error(L"RegGetValueW(Port)", st);
    }

    wchar_t greeting[64];
    size = sizeof greeting;
    st = RegGetValueW(key, NULL, L"Greeting", RRF_RT_REG_SZ, &type, greeting, &size);
    if (st == ERROR_SUCCESS) {
        wprintf(L"Greeting = %ls (type %lu)\n", greeting, (unsigned long)type);
    } else {
        print_error(L"RegGetValueW(Greeting)", st);
    }

    /* REG_EXPAND_SZ 默认展开：展开之后类型按 REG_SZ 返回。 */
    wchar_t dir[256];
    size = sizeof dir;
    st = RegGetValueW(key, NULL, L"DataDir", RRF_RT_ANY, &type, dir, &size);
    if (st == ERROR_SUCCESS) {
        wprintf(L"DataDir  = %ls (type %lu, expanded)\n", dir, (unsigned long)type);
    } else {
        print_error(L"RegGetValueW(DataDir)", st);
    }

    /* 加 RRF_NOEXPAND 才原样取回 %LOCALAPPDATA%。 */
    size = sizeof dir;
    st = RegGetValueW(key, NULL, L"DataDir", RRF_RT_ANY | RRF_NOEXPAND, &type, dir, &size);
    if (st == ERROR_SUCCESS) {
        wprintf(L"DataDir  = %ls (type %lu, RRF_NOEXPAND)\n", dir, (unsigned long)type);
    } else {
        print_error(L"RegGetValueW(DataDir, noexpand)", st);
    }

    /* 展开之后类型按 REG_SZ 返回，所以只允许 REG_EXPAND_SZ 反而读不到。 */
    size = sizeof dir;
    st = RegGetValueW(key, NULL, L"DataDir", RRF_RT_REG_EXPAND_SZ, NULL, dir, &size);
    wprintf(L"DataDir restricted to REG_EXPAND_SZ -> %ld\n", (long)st);

    /* 故意用字符串类型去读 DWORD 值，观察返回的错误码。 */
    size = sizeof dir;
    st = RegGetValueW(key, NULL, L"Port", RRF_RT_REG_SZ, NULL, dir, &size);
    wprintf(L"Port as REG_SZ -> %ld\n", (long)st);
}

static void enum_values(HKEY key) {
    DWORD index = 0;
    wchar_t name[256];
    BYTE data[256];
    DWORD name_len;
    DWORD data_len;
    DWORD type;
    LSTATUS st;

    for (;;) {
        name_len = 256;
        data_len = sizeof data;
        st = RegEnumValueW(key, index, name, &name_len, NULL, &type, data, &data_len);
        if (st == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (st != ERROR_SUCCESS) {
            print_error(L"RegEnumValueW", st);
            break;
        }
        wprintf(L"  [%lu] %-10ls type %lu, %lu bytes\n", (unsigned long)index, name,
                (unsigned long)type, (unsigned long)data_len);
        index++;
    }
}

int main(int argc, char *argv[]) {
    HKEY key = NULL;
    DWORD disposition = 0;
    LSTATUS st;
    int keep = argc > 1 && strcmp(argv[1], "keep") == 0;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    st = RegCreateKeyExW(HKEY_CURRENT_USER, CONFIG_KEY, 0, NULL, REG_OPTION_NON_VOLATILE,
                         KEY_ALL_ACCESS, NULL, &key, &disposition);
    if (st != ERROR_SUCCESS) {
        print_error(L"RegCreateKeyExW", st);
        return 1;
    }
    wprintf(L"key created, disposition %lu (1 = new, 2 = existing)\n",
            (unsigned long)disposition);

    st = write_config(key);
    if (st != ERROR_SUCCESS) {
        print_error(L"RegSetValueExW", st);
        RegCloseKey(key);
        return 1;
    }

    read_config(key);

    wprintf(L"values under HKCU\\%ls:\n", CONFIG_KEY);
    enum_values(key);
    RegCloseKey(key);

    if (keep) {
        wprintf(L"key kept, inspect it with: reg query HKCU\\Software\\MdrTutorial\\Echo\n");
        return 0;
    }

    st = RegDeleteTreeW(HKEY_CURRENT_USER, CONFIG_KEY);
    wprintf(L"RegDeleteTreeW -> %ld\n", (long)st);
    return st == ERROR_SUCCESS ? 0 : 1;
}