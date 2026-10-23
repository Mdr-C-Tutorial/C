#include "textconv.h"

#include <stdlib.h>
#include <windows.h>

wchar_t *utf8_to_wide(const char *utf8) {
    int need = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (need == 0) {
        return NULL;
    }
    wchar_t *wide = malloc((size_t)need * sizeof *wide);
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide, need) == 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

char *wide_to_utf8(const wchar_t *wide) {
    int need = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
    if (need == 0) {
        return NULL;
    }
    char *utf8 = malloc((size_t)need);
    if (utf8 == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, utf8, need, NULL, NULL) == 0) {
        free(utf8);
        return NULL;
    }
    return utf8;
}