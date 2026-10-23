#include "mdr_compiler.h"
#include "mdr_ext.h"
#include "mdr_memory.h"
#include "mdr_string.h"

#include <stdlib.h>

/* 21.5 的扩展函数：名字、语义都取自已广泛使用的 POSIX/BSD/GNU 扩展，
 * 用 mdr_ 前缀提供，各平台行为一致。 */

char *mdr_strdup(const char *s) {
    size_t len = mdr_strlen(s);
    char *copy = (char *)malloc(len + 1);

    if (copy == NULL) {
        return NULL;
    }
    mdr_memcpy(copy, s, len + 1);
    return copy;
}

char *mdr_strndup(const char *s, size_t n) {
    size_t len = mdr_strnlen(s, n);
    char *copy = (char *)malloc(len + 1);

    if (copy == NULL) {
        return NULL;
    }
    mdr_memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

char *mdr_stpcpy(char *restrict dest, const char *restrict src) {
    while ((*dest = *src) != '\0') {
        dest++;
        src++;
    }
    return dest;
}

size_t mdr_strlcpy(char *restrict dst, const char *restrict src, size_t dsize) {
    size_t len = mdr_strlen(src);

    if (dsize > 0) {
        size_t copy = len < dsize - 1 ? len : dsize - 1;

        mdr_memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return len;
}

size_t mdr_strlcat(char *restrict dst, const char *restrict src, size_t dsize) {
    size_t dlen = mdr_strnlen(dst, dsize);
    size_t slen = mdr_strlen(src);
    size_t copy;

    if (dlen == dsize) {
        return dsize + slen;
    }
    copy = dsize - dlen - 1;
    if (copy > slen) {
        copy = slen;
    }
    mdr_memcpy(dst + dlen, src, copy);
    dst[dlen + copy] = '\0';
    return dlen + slen;
}

void *mdr_memmem(const void *haystack, size_t haystacklen, const void *needle,
                 size_t needlelen) {
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    size_t remaining = haystacklen;

    if (needlelen == 0) {
        return (void *)haystack;
    }
    while (remaining >= needlelen) {
        const unsigned char *p = (const unsigned char *)mdr_memchr(
            h, n[0], remaining - needlelen + 1);
        size_t skipped;

        if (p == NULL) {
            break;
        }
        if (mdr_memcmp(p, n, needlelen) == 0) {
            return (void *)p;
        }
        skipped = (size_t)(p - h) + 1;
        h += skipped;
        remaining -= skipped;
    }
    return NULL;
}

char *mdr_strtok_r(char *restrict str, const char *restrict delim,
                   char **restrict saveptr) {
    char *start;

    if (str != NULL) {
        *saveptr = str;
    }
    if (*saveptr == NULL) {
        return NULL;
    }
    start = *saveptr + mdr_strspn(*saveptr, delim);
    if (*start == '\0') {
        *saveptr = NULL;
        return NULL;
    }
    *saveptr = start + mdr_strcspn(start, delim);
    if (**saveptr != '\0') {
        **saveptr = '\0';
        (*saveptr)++;
    } else {
        *saveptr = NULL;
    }
    return start;
}

char *mdr_strsep(char **restrict stringp, const char *restrict delim) {
    char *start = *stringp;

    if (start == NULL) {
        return NULL;
    }
    *stringp = start + mdr_strcspn(start, delim);
    if (**stringp != '\0') {
        **stringp = '\0';
        (*stringp)++;
    } else {
        *stringp = NULL;
    }
    return start;
}

static unsigned char ascii_lower(unsigned char c) {
    if (c >= 'A' && c <= 'Z') {
        return (unsigned char)(c - 'A' + 'a');
    }
    return c;
}

int mdr_ascii_casecmp(const char *s1, const char *s2) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (*p1 != '\0' && ascii_lower(*p1) == ascii_lower(*p2)) {
        p1++;
        p2++;
    }
    return (int)ascii_lower(*p1) - (int)ascii_lower(*p2);
}

int mdr_ascii_ncasecmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (n > 0 && *p1 != '\0' && ascii_lower(*p1) == ascii_lower(*p2)) {
        p1++;
        p2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (int)ascii_lower(*p1) - (int)ascii_lower(*p2);
}