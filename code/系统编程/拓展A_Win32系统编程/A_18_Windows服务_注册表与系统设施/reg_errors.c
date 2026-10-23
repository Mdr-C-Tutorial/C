/* 注册表读写里容易出错的几处：缓冲区不够、类型限定、缺失的项与值。
 *
 * 构建：gcc -std=c17 -Wall -Wextra -o reg_errors.exe reg_errors.c
 */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

#define PROBE_KEY L"Software\\MdrTutorial\\Probe"

int main(void) {
    HKEY key = NULL;
    const wchar_t greeting[] = L"hello";
    const wchar_t data_dir[] = L"%LOCALAPPDATA%\\MdrTutorial\\Echo";
    wchar_t buf[64];
    DWORD size;
    DWORD type;
    DWORD number = 42;
    LSTATUS st;

    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);

    RegCreateKeyExW(HKEY_CURRENT_USER, PROBE_KEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"Greeting", 0, REG_SZ, (const BYTE *)greeting, (DWORD)sizeof greeting);
    RegSetValueExW(key, L"Number", 0, REG_DWORD, (const BYTE *)&number, sizeof number);
    RegSetValueExW(key, L"DataDir", 0, REG_EXPAND_SZ, (const BYTE *)data_dir,
                   (DWORD)sizeof data_dir);

    /* 缓冲区不够：返回 234（ERROR_MORE_DATA），同时给出需要的字节数。 */
    size = 4;
    st = RegGetValueW(key, NULL, L"Greeting", RRF_RT_REG_SZ, &type, buf, &size);
    wprintf(L"4-byte buffer              -> %ld, needs %lu bytes\n", (long)st,
            (unsigned long)size);

    /* 用字符串类型读 DWORD 值：类型不符，1630（ERROR_UNSUPPORTED_TYPE）。 */
    size = sizeof buf;
    st = RegGetValueW(key, NULL, L"Number", RRF_RT_REG_SZ, &type, buf, &size);
    wprintf(L"read DWORD as REG_SZ      -> %ld\n", (long)st);

    /* REG_EXPAND_SZ 默认展开，展开后的类型按 REG_SZ 返回；
       只允许 REG_EXPAND_SZ 反而读不到，加 RRF_NOEXPAND 才能拿到原样。 */
    size = sizeof buf;
    st = RegGetValueW(key, NULL, L"DataDir", RRF_RT_REG_EXPAND_SZ, &type, buf, &size);
    wprintf(L"REG_EXPAND_SZ only         -> %ld\n", (long)st);
    size = sizeof buf;
    st = RegGetValueW(key, NULL, L"DataDir", RRF_RT_ANY | RRF_NOEXPAND, &type, buf, &size);
    wprintf(L"RRF_NOEXPAND               -> %ld, type %lu, \"%ls\"\n", (long)st,
            (unsigned long)type, buf);

    /* 项和值不存在时都是 2（ERROR_FILE_NOT_FOUND）。 */
    DWORD value = 0;
    size = sizeof value;
    st = RegGetValueW(HKEY_CURRENT_USER, L"Software\\MdrTutorial\\NoSuchKey", L"Port",
                      RRF_RT_REG_DWORD, NULL, &value, &size);
    wprintf(L"missing key                -> %ld\n", (long)st);
    size = sizeof value;
    st = RegGetValueW(key, NULL, L"NoSuchValue", RRF_RT_REG_DWORD, NULL, &value, &size);
    wprintf(L"missing value              -> %ld\n", (long)st);

    RegCloseKey(key);
    RegDeleteTreeW(HKEY_CURRENT_USER, PROBE_KEY);
    return 0;
}