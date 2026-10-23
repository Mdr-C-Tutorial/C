#include "mdr_compiler.h"
#include "mdr_string.h"

#include <stdbool.h>

size_t mdr_strlen(const char *s) {
    size_t len = 0;

    MDR_ASSERT(s != NULL);
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

int mdr_strcmp(const char *lhs, const char *rhs) {
    const unsigned char *p1 = (const unsigned char *)lhs;
    const unsigned char *p2 = (const unsigned char *)rhs;

    MDR_ASSERT(lhs != NULL);
    MDR_ASSERT(rhs != NULL);
    while (*p1 != '\0' && *p1 == *p2) {
        p1++;
        p2++;
    }
    return (int)*p1 - (int)*p2;
}

size_t mdr_strnlen(const char *s, size_t maxlen) {
    size_t len = 0;

    MDR_ASSERT(s != NULL);
    while (len < maxlen && s[len] != '\0') {
        len++;
    }
    return len;
}

char *mdr_strcpy(char *restrict dest, const char *restrict src) {
    char *d = dest;

    MDR_ASSERT(dest != NULL);
    MDR_ASSERT(src != NULL);
    while ((*d = *src) != '\0') {
        d++;
        src++;
    }
    return dest;
}

char *mdr_strncpy(char *restrict dest, const char *restrict src, size_t count) {
    size_t i = 0;

    MDR_ASSERT(dest != NULL);
    MDR_ASSERT(src != NULL);
    for (; i < count && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < count; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char *mdr_strcat(char *restrict dest, const char *restrict src) {
    char *d;

    MDR_ASSERT(dest != NULL);
    MDR_ASSERT(src != NULL);
    d = dest + mdr_strlen(dest);
    mdr_strcpy(d, src);
    return dest;
}

char *mdr_strncat(char *restrict dest, const char *restrict src, size_t count) {
    char *d;

    MDR_ASSERT(dest != NULL);
    MDR_ASSERT(src != NULL);
    d = dest + mdr_strlen(dest);
    while (count > 0 && *src != '\0') {
        *d = *src;
        d++;
        src++;
        count--;
    }
    *d = '\0';
    return dest;
}

int mdr_strncmp(const char *lhs, const char *rhs, size_t count) {
    const unsigned char *p1 = (const unsigned char *)lhs;
    const unsigned char *p2 = (const unsigned char *)rhs;

    MDR_ASSERT(lhs != NULL);
    MDR_ASSERT(rhs != NULL);
    while (count > 0 && *p1 != '\0' && *p1 == *p2) {
        p1++;
        p2++;
        count--;
    }
    if (count == 0) {
        return 0;
    }
    return (int)*p1 - (int)*p2;
}

char *mdr_strchr(const char *s, int ch) {
    char c = (char)ch;

    MDR_ASSERT(s != NULL);
    while (*s != '\0' && *s != c) {
        s++;
    }
    if (*s == c) {
        return (char *)s;
    }
    return NULL;
}

char *mdr_strrchr(const char *s, int ch) {
    const char *last = NULL;
    char c = (char)ch;

    MDR_ASSERT(s != NULL);
    do {
        if (*s == c) {
            last = s;
        }
    } while (*s++ != '\0');
    return (char *)last;
}

size_t mdr_strspn(const char *s, const char *accept) {
    bool in_set[256] = {false};
    size_t n = 0;

    MDR_ASSERT(s != NULL);
    MDR_ASSERT(accept != NULL);
    while (*accept != '\0') {
        in_set[(unsigned char)*accept] = true;
        accept++;
    }
    while (s[n] != '\0' && in_set[(unsigned char)s[n]]) {
        n++;
    }
    return n;
}

size_t mdr_strcspn(const char *s, const char *reject) {
    bool in_set[256] = {false};
    size_t n = 0;

    MDR_ASSERT(s != NULL);
    MDR_ASSERT(reject != NULL);
    while (*reject != '\0') {
        in_set[(unsigned char)*reject] = true;
        reject++;
    }
    while (s[n] != '\0' && !in_set[(unsigned char)s[n]]) {
        n++;
    }
    return n;
}

char *mdr_strpbrk(const char *s, const char *breakset) {
    size_t n = mdr_strcspn(s, breakset);

    if (s[n] == '\0') {
        return NULL;
    }
    return (char *)(s + n);
}

char *mdr_strstr(const char *haystack, const char *needle) {
    size_t nlen = mdr_strlen(needle);
    const char *p = haystack;

    MDR_ASSERT(haystack != NULL);
    MDR_ASSERT(needle != NULL);
    if (nlen == 0) {
        return (char *)haystack;
    }
    while ((p = mdr_strchr(p, needle[0])) != NULL) {
        if (mdr_strncmp(p, needle, nlen) == 0) {
            return (char *)p;
        }
        p++;
    }
    return NULL;
}

char *mdr_strtok(char *restrict str, const char *restrict delim) {
    static char *next;
    char *start;
    char *end;

    MDR_ASSERT(delim != NULL);
    if (str != NULL) {
        next = str;
    }
    if (next == NULL) {
        return NULL;
    }
    start = next + mdr_strspn(next, delim);
    if (*start == '\0') {
        next = NULL;
        return NULL;
    }
    end = start + mdr_strcspn(start, delim);
    if (*end != '\0') {
        *end = '\0';
        next = end + 1;
    } else {
        next = NULL;
    }
    return start;
}
