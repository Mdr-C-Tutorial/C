#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LOG_NAME "hup_demo.log"

static void sleep_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};

    nanosleep(&ts, NULL);
}

static void log_line(const char *what) {
    int fd = open(LOG_NAME, O_WRONLY | O_CREAT | O_APPEND, 0644);
    char line[96];
    int n = snprintf(line, sizeof line, "%-7s pid %d pgid %d sid %d\n", what, (int)getpid(),
                     (int)getpgrp(), (int)getsid(0));

    if (fd == -1) {
        return;
    }
    if (write(fd, line, (size_t)n) < 0) {
    }
    close(fd);
}

/* 首进程的处理函数：记一笔，按模式决定是否退出 */
static int leader_exits_on_hup;

static void leader_on_hup(int sig) {
    (void)sig;
    log_line("SIGHUP");
    if (leader_exits_on_hup) {
        _exit(0);
    }
}

static void child_on_hup(int sig) {
    (void)sig;
    log_line("SIGHUP");
}

int main(int argc, char *argv[]) {
    const char *mode = argc > 1 ? argv[1] : "stay";

    leader_exits_on_hup = strcmp(mode, "exit") == 0;
    unlink(LOG_NAME);

    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master == -1) {
        perror("posix_openpt");
        return EXIT_FAILURE;
    }
    grantpt(master);
    unlockpt(master);
    char *name = ptsname(master);
    pid_t leader = fork();

    if (leader == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (leader == 0) {
        setsid();
        signal(SIGHUP, leader_on_hup);
        int slave = open(name, O_RDWR);
        if (slave == -1) {
            _exit(127);
        }
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        close(slave);
        close(master);

        /* 子进程另建一个前台进程组 */
        pid_t child = fork();
        if (child == 0) {
            signal(SIGHUP, child_on_hup);
            signal(SIGTTOU, SIG_IGN);
            setpgid(0, 0);
            if (tcsetpgrp(STDIN_FILENO, getpid()) == -1) {
                _exit(1);
            }
            log_line("fg-ready");
            for (;;) {
                pause();
            }
        }
        log_line("leader");
        for (;;) {
            pause();
        }
    }

    sleep_ms(300);
    printf("mode %s, closing master\n", mode);
    close(master);
    sleep_ms(600);
    kill(-leader, SIGKILL);
    kill(leader, SIGKILL);
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }

    FILE *fp = fopen(LOG_NAME, "r");
    if (fp != NULL) {
        char line[128];
        while (fgets(line, sizeof line, fp) != NULL) {
            fputs(line, stdout);
        }
        fclose(fp);
    }
    return 0;
}