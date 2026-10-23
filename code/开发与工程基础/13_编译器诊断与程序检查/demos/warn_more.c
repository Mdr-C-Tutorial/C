#include <stdio.h>

int open_file();

int double_value(int x) {
    return x * 2;
}

int open_file() {
    int count = 1;
    {
        int count = 2;
        printf("%d\n", count);
    }
    int values[count];
    for (int i = 0; i < count; i++) {
        values[i] = i;
    }
    return values[0];
}

static void log_value(const char *fmt, int value) {
    printf(fmt, value);
}

int main(void) {
    log_value("%d\n", double_value(open_file()));
    return 0;
}
