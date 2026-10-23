#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>

static void show_flag(const char *when) {
    FILE *fp = fopen("/proc/self/status", "re");
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    char line[256];
    while (fgets(line, sizeof line, fp) != NULL) {
        if (strncmp(line, "NoNewPrivs:", 11) == 0) {
            printf("%s: %s", when, line);
            break;
        }
    }
    fclose(fp);
}

int main(void) {
    show_flag("before");
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
        perror("prctl");
        return 1;
    }
    show_flag("after ");
    return 0;
}