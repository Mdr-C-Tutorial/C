#define _POSIX_C_SOURCE 200809L

/*
 * sensord：一个最小但完整的用户态服务。
 *
 *   - 前台运行，日志写标准错误，统计结果写标准输出；
 *   - 周期读取数据源（普通文件或命名管道），每个周期输出一行统计；
 *   - SIGHUP 重读配置文件并立即生效，SIGTERM/SIGINT 干净退出；
 *   - 用第 12 章的事件循环库 evl 管理描述符和定时器。
 */

#include "evl.h"
#include "sdnotify.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PATH_LEN 256
#define LINE_LEN 256

struct config {
    char source[PATH_LEN];
    long period_ms;
};

struct window {
    long count;
    double sum;
    double min;
    double max;
};

struct sensord {
    evl_loop *loop;
    struct config cfg;
    const char *conf_path;
    int src_fd;
    bool src_stream;
    evl_timer *timer;
    struct window win;
    char line[LINE_LEN];
    size_t line_len;
};

static void logmsg(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

static void config_defaults(struct config *cfg) {
    snprintf(cfg->source, sizeof cfg->source, "sensors.txt");
    cfg->period_ms = 1000;
}

/* 去掉首尾空白，返回指向字符串内部的指针。 */
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    char *end = s + strlen(s);

    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                       end[-1] == '\r')) {
        end--;
    }
    *end = '\0';
    return s;
}

static int config_load(const char *path, struct config *cfg) {
    FILE *fp = fopen(path, "r");

    if (fp == NULL) {
        logmsg("open %s: %s", path, strerror(errno));
        return -1;
    }
    struct config next;
    char buf[LINE_LEN];

    config_defaults(&next);
    while (fgets(buf, sizeof buf, fp) != NULL) {
        char *line = trim(buf);

        if (*line == '\0' || *line == '#') {
            continue;
        }
        char *eq = strchr(line, '=');

        if (eq == NULL) {
            logmsg("%s: 忽略没有等号的行: %s", path, line);
            continue;
        }
        *eq = '\0';
        char *key = trim(line);
        char *value = trim(eq + 1);

        if (strcmp(key, "source") == 0) {
            snprintf(next.source, sizeof next.source, "%s", value);
        } else if (strcmp(key, "period_ms") == 0) {
            char *end = NULL;
            long period = strtol(value, &end, 10);

            if (end == value || *end != '\0' || period <= 0 || period > 60000) {
                logmsg("%s: period_ms 取值无效: %s", path, value);
                fclose(fp);
                return -1;
            }
            next.period_ms = period;
        } else {
            logmsg("%s: 忽略未知配置项 %s", path, key);
        }
    }
    fclose(fp);
    *cfg = next;
    return 0;
}

/*
 * 以读写方式打开命名管道：进程自己也是写端，写端关闭时读端不会看到
 * 文件结束，事件循环里也就不必处理反复重开的循环。普通文件只读打开。
 */
static int source_open(const char *path, bool *is_stream) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);

    *is_stream = false;
    if (fd < 0) {
        logmsg("open %s: %s", path, strerror(errno));
        return -1;
    }
    struct stat st;

    if (fstat(fd, &st) == 0 && S_ISFIFO(st.st_mode)) {
        close(fd);
        fd = open(path, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            logmsg("open %s: %s", path, strerror(errno));
            return -1;
        }
        *is_stream = true;
    }
    return fd;
}

static void window_reset(struct window *win) {
    win->count = 0;
    win->sum = 0.0;
    win->min = 0.0;
    win->max = 0.0;
}

static void window_add(struct window *win, double value) {
    if (win->count == 0) {
        win->min = value;
        win->max = value;
    } else {
        if (value < win->min) {
            win->min = value;
        }
        if (value > win->max) {
            win->max = value;
        }
    }
    win->count++;
    win->sum += value;
}

/* 处理一行：第一列是时间戳，第二列是读数，其余列忽略。 */
static void handle_line(struct sensord *srv, const char *line) {
    char copy[LINE_LEN];

    snprintf(copy, sizeof copy, "%s", line);
    char *save = NULL;
    char *stamp = strtok_r(copy, " \t,", &save);

    if (stamp == NULL) {
        return;
    }
    char *reading = strtok_r(NULL, " \t,", &save);

    if (reading == NULL) {
        logmsg("跳过不完整的数据行: %s", line);
        return;
    }
    char *end = NULL;
    double value = strtod(reading, &end);

    if (end == reading) {
        logmsg("跳过无法解析的数据行: %s", line);
        return;
    }
    window_add(&srv->win, value);
}

static void drain_source(struct sensord *srv) {
    char buf[512];

    for (;;) {
        ssize_t n = read(srv->src_fd, buf, sizeof buf);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            logmsg("read %s: %s", srv->cfg.source, strerror(errno));
            return;
        }
        if (n == 0) {
            return;
        }
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];

            if (c == '\n') {
                srv->line[srv->line_len] = '\0';
                handle_line(srv, srv->line);
                srv->line_len = 0;
            } else if (srv->line_len + 1 < sizeof srv->line) {
                srv->line[srv->line_len++] = c;
            }
        }
    }
}

static void report(struct sensord *srv) {
    struct timespec ts;
    char stamp[32];
    struct tm tm;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    strftime(stamp, sizeof stamp, "%H:%M:%S", &tm);
    if (srv->win.count == 0) {
        printf("%s samples=0\n", stamp);
    } else {
        printf("%s samples=%ld mean=%.2f min=%.2f max=%.2f\n", stamp, srv->win.count,
               srv->win.sum / (double)srv->win.count, srv->win.min, srv->win.max);
    }
    fflush(stdout);
    window_reset(&srv->win);
}

static void on_tick(evl_loop *loop, evl_timer *timer, void *arg) {
    struct sensord *srv = arg;

    (void)loop;
    (void)timer;
    drain_source(srv);
    report(srv);
    /* 一轮工作做完才喂狗：统计输出了，说明采集与计算都正常。 */
    (void)notify_send("WATCHDOG=1\n");
}

static void on_source(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct sensord *srv = arg;

    (void)loop;
    (void)fd;
    (void)events;
    drain_source(srv);
}

static bool reopen_source(struct sensord *srv) {
    bool is_stream;
    int fd = source_open(srv->cfg.source, &is_stream);

    if (fd < 0) {
        return false;
    }
    if (srv->src_stream) {
        evl_del_fd(srv->loop, srv->src_fd);
    }
    close(srv->src_fd);
    srv->src_fd = fd;
    srv->src_stream = is_stream;
    if (is_stream && evl_add_fd(srv->loop, srv->src_fd, EVL_READ, on_source, srv) < 0) {
        logmsg("evl_add_fd: %s", strerror(errno));
        return false;
    }
    return true;
}

static void reload(struct sensord *srv) {
    struct config next;

    if (config_load(srv->conf_path, &next) < 0) {
        logmsg("重载失败，继续使用旧配置");
        return;
    }
    bool reopen = strcmp(next.source, srv->cfg.source) != 0;
    bool period_changed = next.period_ms != srv->cfg.period_ms;

    srv->cfg = next;
    if (reopen) {
        reopen_source(srv);
    }
    if (period_changed) {
        evl_cancel_timer(srv->loop, srv->timer);
        srv->timer = evl_add_timer(srv->loop, srv->cfg.period_ms, srv->cfg.period_ms,
                                   on_tick, srv);
    }
    logmsg("重载配置: source=%s period_ms=%ld", srv->cfg.source, srv->cfg.period_ms);
}

static void on_signal(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct sensord *srv = arg;
    struct signalfd_siginfo info;

    (void)events;
    while (read(fd, &info, sizeof info) == sizeof info) {
        if (info.ssi_signo == SIGTERM || info.ssi_signo == SIGINT) {
            logmsg("收到信号 %u，退出", info.ssi_signo);
            evl_stop(loop);
        } else if (info.ssi_signo == SIGHUP) {
            reload(srv);
        }
    }
}

int main(int argc, char **argv) {
    const char *conf_path = "sensord.conf";
    struct sensord srv;
    int opt;

    while ((opt = getopt(argc, argv, "f:")) != -1) {
        if (opt == 'f') {
            conf_path = optarg;
        } else {
            fprintf(stderr, "用法: %s [-f 配置文件]\n", argv[0]);
            return 2;
        }
    }
    memset(&srv, 0, sizeof srv);
    srv.conf_path = conf_path;
    config_defaults(&srv.cfg);
    if (config_load(conf_path, &srv.cfg) < 0) {
        return 1;
    }
    srv.src_fd = source_open(srv.cfg.source, &srv.src_stream);
    if (srv.src_fd < 0) {
        return 1;
    }
    srv.loop = evl_create();
    if (srv.loop == NULL) {
        logmsg("evl_create: %s", strerror(errno));
        return 1;
    }
    window_reset(&srv.win);
    /* 普通文件永远处于就绪状态，交给 epoll 只会空转，所以只在命名管道上登记。 */
    if (srv.src_stream && evl_add_fd(srv.loop, srv.src_fd, EVL_READ, on_source, &srv) < 0) {
        logmsg("evl_add_fd: %s", strerror(errno));
        return 1;
    }

    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        logmsg("sigprocmask: %s", strerror(errno));
        return 1;
    }
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);

    if (sfd < 0) {
        logmsg("signalfd: %s", strerror(errno));
        return 1;
    }
    if (evl_add_fd(srv.loop, sfd, EVL_READ, on_signal, &srv) < 0) {
        logmsg("evl_add_fd: %s", strerror(errno));
        return 1;
    }
    srv.timer = evl_add_timer(srv.loop, srv.cfg.period_ms, srv.cfg.period_ms, on_tick,
                              &srv);
    if (srv.timer == NULL) {
        logmsg("evl_add_timer: %s", strerror(errno));
        return 1;
    }
    logmsg("sensord 启动: pid=%ld source=%s period_ms=%ld", (long)getpid(),
           srv.cfg.source, srv.cfg.period_ms);
    /* 配合 Type=notify：告诉服务管理器启动完成。没有 systemd 时什么也不做。 */
    (void)notify_send("READY=1\nSTATUS=sensord started");
    if (evl_run(srv.loop) < 0) {
        logmsg("evl_run: %s", strerror(errno));
    }
    evl_destroy(srv.loop);
    close(sfd);
    close(srv.src_fd);
    return 0;
}