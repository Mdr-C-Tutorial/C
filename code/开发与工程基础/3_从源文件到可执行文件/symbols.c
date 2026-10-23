#include <stdio.h>

int counter = 5;
int total;
static int calls = 1;
static int cache;
const int limits[3] = {10, 20, 30};
static const char banner[] = "symbols";

static void log_call(void) {
    calls++;
    cache = calls;
}

int add(int a, int b) {
    log_call();
    total = a + b;
    return total;
}

int main(void) {
    puts(banner);
    return add(counter, limits[0]) > 0 ? 0 : 1;
}
