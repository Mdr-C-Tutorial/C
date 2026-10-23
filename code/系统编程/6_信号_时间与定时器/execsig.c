#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

/* 读 /proc/self/status 里的 SigBlk/SigIgn/SigCgt，按信号逐个解码。 */
static void print_state(const char *label) {
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp == NULL) {
        die("fopen status");
    }
    unsigned long long blk = 0, ign = 0, cgt = 0;
    char line[256];
    while (fgets(line, sizeof line, fp) != NULL) {
        unsigned long long value;
        if (sscanf(line, "SigBlk: %llx", &value) == 1) {
            blk = value;
        } else if (sscanf(line, "SigIgn: %llx", &value) == 1) {
            ign = value;
        } else if (sscanf(line, "SigCgt: %llx", &value) == 1) {
            cgt = value;
        }
    }
    fclose(fp);

    printf("%s\n", label);
    static const int shown[] = {SIGINT, SIGTERM, SIGUSR1, SIGUSR2, SIGCHLD};
    static const char *names[] = {"SIGINT", "SIGTERM", "SIGUSR1", "SIGUSR2",
                                  "SIGCHLD"};
    for (size_t i = 0; i < sizeof shown / sizeof shown[0]; i++) {
        unsigned long long bit = 1ULL << (shown[i] - 1);
        printf("  %-8s blocked=%d ignored=%d caught=%d\n", names[i],
               (blk & bit) != 0, (ign & bit) != 0, (cgt & bit) != 0);
    }
}

static void on_term(int signo) {
    (void)signo;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        print_state("after exec:");
        return 0;
    }

    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_term;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        die("sigaction SIGUSR1");
    }
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        die("sigaction SIGUSR2");
    }

    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &block, NULL) == -1) {
        die("sigprocmask");
    }

    print_state("before exec:");
    fflush(stdout);

    execl(argv[0], argv[0], "child", (char *)NULL);
    die("execl");
    return 0;
}