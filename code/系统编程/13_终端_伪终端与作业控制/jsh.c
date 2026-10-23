#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define MAX_JOBS 16
#define MAX_ARGS 32

enum job_state { JOB_RUNNING, JOB_STOPPED };

struct job {
    int used;
    pid_t pgid;
    enum job_state state;
    struct termios tmodes;
    char command[128];
};

static struct job jobs[MAX_JOBS];
static struct termios shell_tmodes;
static pid_t shell_pgid;
static int tty_fd = STDIN_FILENO;

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void set_signal(int sig, void (*handler)(int)) {
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sig, &sa, NULL) == -1) {
        die("sigaction");
    }
}

static int find_job(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].used && jobs[i].pgid == pid) {
            return i;
        }
    }
    return -1;
}

static void format_command(char *dst, size_t size, int argc, char *argv[]) {
    size_t off = 0;

    dst[0] = '\0';
    for (int i = 0; i < argc; i++) {
        int n = snprintf(dst + off, size - off, "%s%s", i == 0 ? "" : " ", argv[i]);

        if (n < 0 || (size_t)n >= size - off) {
            break;
        }
        off += (size_t)n;
    }
}

static void report_status(int number, int status) {
    const char *cmd = jobs[number].command;

    if (WIFSTOPPED(status)) {
        jobs[number].state = JOB_STOPPED;
        /* 作业停在这里时终端还归它所有，此刻读到的就是它用的模式 */
        tcgetattr(tty_fd, &jobs[number].tmodes);
        printf("[%d]+ stopped: %s\n", number + 1, cmd);
    } else if (WIFCONTINUED(status)) {
        jobs[number].state = JOB_RUNNING;
    } else if (WIFSIGNALED(status)) {
        printf("[%d]+ killed by signal %d (%s): %s\n", number + 1, WTERMSIG(status),
               strsignal(WTERMSIG(status)), cmd);
        jobs[number].used = 0;
    } else if (WIFEXITED(status)) {
        printf("[%d]+ done: %s\n", number + 1, cmd);
        jobs[number].used = 0;
    }
}

static void reap_jobs(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        int i = find_job(pid);

        if (i >= 0) {
            report_status(i, status);
        }
    }
}

static void wait_for_job(int number) {
    int status;
    pid_t r;

    do {
        r = waitpid(-jobs[number].pgid, &status, WUNTRACED);
    } while (r == -1 && errno == EINTR);

    if (r == -1) {
        perror("waitpid");
        jobs[number].used = 0;
    } else {
        report_status(number, status);
    }
    /* 先收回终端，再恢复 shell 自己的终端模式；作业停止时的模式已在 report_status 里存下 */
    if (tcsetpgrp(tty_fd, shell_pgid) == -1) {
        perror("tcsetpgrp");
    }
    if (tcsetattr(tty_fd, TCSADRAIN, &shell_tmodes) == -1) {
        perror("tcsetattr");
    }
}

static void launch(int argc, char *argv[], int foreground) {
    int slot = -1;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        printf("jsh: too many jobs\n");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        setpgid(0, 0);
        if (foreground) {
            tcsetpgrp(tty_fd, getpid());
        }
        set_signal(SIGINT, SIG_DFL);
        set_signal(SIGQUIT, SIG_DFL);
        set_signal(SIGTSTP, SIG_DFL);
        set_signal(SIGTTIN, SIG_DFL);
        set_signal(SIGTTOU, SIG_DFL);
        execvp(argv[0], argv);
        fprintf(stderr, "jsh: %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    setpgid(pid, pid);
    if (foreground) {
        tcsetpgrp(tty_fd, pid);
    }

    jobs[slot].used = 1;
    jobs[slot].pgid = pid;
    jobs[slot].state = JOB_RUNNING;
    format_command(jobs[slot].command, sizeof jobs[slot].command, argc, argv);

    if (foreground) {
        wait_for_job(slot);
    } else {
        printf("[%d] %d\n", slot + 1, pid);
    }
}

static int current_job(void) {
    for (int i = MAX_JOBS - 1; i >= 0; i--) {
        if (jobs[i].used) {
            return i;
        }
    }
    return -1;
}

static int parse_job(char *arg) {
    if (arg == NULL) {
        return current_job();
    }
    if (arg[0] == '%') {
        arg++;
    }
    int n = atoi(arg);

    if (n < 1 || n > MAX_JOBS || !jobs[n - 1].used) {
        return -1;
    }
    return n - 1;
}

static void list_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].used) {
            printf("[%d] %s %s\n", i + 1, jobs[i].state == JOB_STOPPED ? "Stopped" : "Running",
                   jobs[i].command);
        }
    }
}

static void continue_job(char *arg, int foreground) {
    int i = parse_job(arg);

    if (i < 0) {
        printf("jsh: no such job\n");
        return;
    }
    if (foreground && tcsetpgrp(tty_fd, jobs[i].pgid) == -1) {
        perror("tcsetpgrp");
    }
    if (foreground && jobs[i].state == JOB_STOPPED) {
        if (tcsetattr(tty_fd, TCSADRAIN, &jobs[i].tmodes) == -1) {
            perror("tcsetattr");
        }
    }
    if (kill(-jobs[i].pgid, SIGCONT) == -1) {
        perror("kill");
    }
    jobs[i].state = JOB_RUNNING;
    if (foreground) {
        wait_for_job(i);
    } else {
        printf("[%d] running: %s\n", i + 1, jobs[i].command);
    }
}

int main(void) {
    set_signal(SIGINT, SIG_IGN);
    set_signal(SIGQUIT, SIG_IGN);
    set_signal(SIGTSTP, SIG_IGN);
    set_signal(SIGTTIN, SIG_IGN);
    set_signal(SIGTTOU, SIG_IGN);

    shell_pgid = getpid();
    if (getpgrp() != shell_pgid) {
        setpgid(0, 0);
    }
    if (tcsetpgrp(tty_fd, shell_pgid) == -1) {
        perror("tcsetpgrp");
    }
    if (tcgetattr(tty_fd, &shell_tmodes) == -1) {
        perror("tcgetattr");
    }

    char line[256];
    for (;;) {
        reap_jobs();
        printf("jsh> ");
        fflush(stdout);
        if (fgets(line, sizeof line, stdin) == NULL) {
            break;
        }

        char *args[MAX_ARGS + 1];
        int argc = 0;
        int foreground = 1;
        char *save = NULL;

        for (char *tok = strtok_r(line, " \t\n", &save); tok != NULL;
             tok = strtok_r(NULL, " \t\n", &save)) {
            if (strcmp(tok, "&") == 0) {
                foreground = 0;
                continue;
            }
            if (argc == MAX_ARGS) {
                break;
            }
            args[argc++] = tok;
        }
        args[argc] = NULL;
        if (argc == 0) {
            continue;
        }

        if (strcmp(args[0], "exit") == 0) {
            break;
        } else if (strcmp(args[0], "jobs") == 0) {
            list_jobs();
        } else if (strcmp(args[0], "fg") == 0) {
            continue_job(args[1], 1);
        } else if (strcmp(args[0], "bg") == 0) {
            continue_job(args[1], 0);
        } else {
            launch(argc, args, foreground);
        }
    }
    return 0;
}