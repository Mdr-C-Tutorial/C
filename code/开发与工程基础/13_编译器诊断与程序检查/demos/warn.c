#include <stdio.h>

static int classify(int value) {
    switch (value) {
    case 0:
        puts("zero");
        /* 这里漏了 break */
    case 1:
        puts("small");
        break;
    default:
        puts("other");
        break;
    }
    return value;
}

static int sum(const int *items, int n, int scale) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += items[i];
    }
    return total;
}

int main(int argc, char *argv[]) {
    int items[] = {1, 2, 3};
    size_t count = sizeof items / sizeof items[0];
    int limit = argc;
    long wide = sizeof(int);
    int unused_local = 0;

    int flag = 0;
    if (flag = argc) {
        puts("flag is nonzero");
    }
    printf("%d\n", wide);
    if (count > limit) {
        puts("more items than limit");
    }
    printf("%d\n", classify(0) + sum(items, 3, limit));
    return 0;
}
