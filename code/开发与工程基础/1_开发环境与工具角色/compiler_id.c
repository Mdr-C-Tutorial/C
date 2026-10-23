#include <limits.h>
#include <stdio.h>

int main(void) {
#if defined(__clang__)
    printf("compiler: Clang %d.%d.%d\n", __clang_major__, __clang_minor__,
           __clang_patchlevel__);
#elif defined(__GNUC__)
    printf("compiler: GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__,
           __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    printf("compiler: MSVC %d\n", _MSC_VER);
#else
    puts("compiler: unknown");
#endif

    printf("standard: %ldL\n", (long)__STDC_VERSION__);

#if defined(_WIN32)
    puts("platform: Windows");
#elif defined(__linux__)
    puts("platform: Linux");
#elif defined(__APPLE__)
    puts("platform: macOS");
#else
    puts("platform: other");
#endif

    printf("pointer: %d bits\n", (int)(sizeof(void *) * CHAR_BIT));
    return 0;
}
