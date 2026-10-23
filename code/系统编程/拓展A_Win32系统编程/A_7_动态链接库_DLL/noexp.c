#include <stdio.h>
#include <windows.h>

/* 没有任何 __declspec(dllexport)：观察 MinGW 默认导出什么。 */
int noexp_visible(int x) {
    return x * 2;
}

int noexp_other(int x) {
    return x + 1;
}

static int helper(int x) {
    return x - 1;
}

int noexp_uses_static(int x) {
    return helper(x);
}