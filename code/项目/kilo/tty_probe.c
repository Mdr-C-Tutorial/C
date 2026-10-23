/* tty_probe —— 观察 kilo 退出之后伪终端是否还停在 raw 模式。
 *
 * 用法：tty_probe [kill]
 *   不带参数：通过伪终端向 kilo 发送 Ctrl-Q，让它走正常退出路径。
 *   带 kill：  运行半秒后向 kilo 发送 SIGKILL。
 * 两次都在 forkpty 建立的主设备上读回 termios 设置并打印。
 *
 * 需要 forkpty，因此用 _XOPEN_SOURCE 700 而不是 _POSIX_C_SOURCE。
 */
#define _XOPEN_SOURCE 700

#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static void show(int fd, const char *when) {
    struct termios t;

    if (tcgetattr(fd, &t) == -1) {
        perror("tcgetattr");
        return;
    }
    printf("%-12s ECHO=%d ICANON=%d ISIG=%d OPOST=%d VMIN=%d VTIME=%d\n", when,
           (t.c_lflag & ECHO) != 0, (t.c_lflag & ICANON) != 0,
           (t.c_lflag & ISIG) != 0, (t.c_oflag & OPOST) != 0, t.c_cc[VMIN],
           t.c_cc[VTIME]);
}

static void wait_ms(long ms) {
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};

    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    int hard_kill = argc > 1 && strcmp(argv[1], "kill") == 0;
    struct winsize ws = {.ws_row = 24, .ws_col = 80};
    int master;
    pid_t pid = forkpty(&master, NULL, NULL, &ws);

    if (pid == -1) {
        perror("forkpty");
        return 1;
    }
    if (pid == 0) {
        execl("./kilo", "kilo", "t.c", (char *)NULL);
        perror("execl");
        _exit(127);
    }
    wait_ms(500);
    show(master, "kilo 运行中");
    if (hard_kill) {
        kill(pid, SIGKILL);
    } else {
        write(master, "\x11", 1);
    }
    waitpid(pid, NULL, 0);
    wait_ms(200);
    show(master, "kilo 退出后");
    return 0;
}