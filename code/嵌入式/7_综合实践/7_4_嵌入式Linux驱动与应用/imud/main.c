#define _POSIX_C_SOURCE 200809L

/*
 * imud —— 嵌入式 Linux 上的 IMU 服务。
 *
 * 数据通路：/dev/mdr_imu（或命名管道、普通文件）→ sample_reader →
 * 互补滤波 → 每个周期聚合一条 → MQTT 发布。信号走 signalfd，
 * 采样与配置事件都由 evl 事件循环调度，和 5.10 的 sensord 同一个骨架。
 *
 * 用法：imud [-f 配置文件] [-s 数据源]
 */

#include "config.h"
#include "evl.h"
#include "filter.h"
#include "imud.h"
#include "mqtt_glue.h"
#include "sample_io.h"

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

#define SAMPLE_BATCH 64
#define MQTT_POLL_MS 100
#define PUMP_MS 50
#define RECONNECT_MIN_MS 1000
#define RECONNECT_MAX_MS 30000

struct imud {
    evl_loop *loop;
    struct imud_config cfg;
    const char *conf_path;
    int source_fd;
    bool source_is_stream;
    struct sample_reader reader;
    struct attitude attitude;
    struct mqtt_client mqtt;
    evl_timer *report_timer;
    evl_timer *poll_timer;
    evl_timer *pump_timer;
    evl_timer *reconnect_timer;
    struct imu_sample last_sample;
    bool have_sample;
    long period_samples;
    double pitch_sum;
    double roll_sum;
    uint32_t seq;
    int reconnect_delay_ms;
};

static void logmsg(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void on_reconnect(evl_loop *loop, evl_timer *timer, void *arg);
static void reconnect_schedule(struct imud *srv, int delay_ms);
static int source_open(struct imud *srv, const char *path);
static void on_source_readable(evl_loop *loop, int fd, unsigned events,
                               void *arg);
static void sample_once(struct imud *srv);

/* ---------- 数据源 ---------- */

static int source_open(struct imud *srv, const char *path) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    struct stat st;

    srv->source_is_stream = false;
    if (fd < 0) {
        logmsg("open %s: %s", path, strerror(errno));
        return -1;
    }
    if (fstat(fd, &st) == 0 && S_ISFIFO(st.st_mode)) {
        close(fd);
        fd = open(path, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            logmsg("open %s: %s", path, strerror(errno));
            return -1;
        }
        srv->source_is_stream = true;
    }
    /*
     * 只有命名管道登记进 epoll。字符设备与普通文件都交给采样定时器
     * 读（见 on_pump）：普通文件永远就绪，登记进去会空转；而没有中断
     * 的 /dev/mdr_imu 在驱动的 .poll 里也永远报告可读，同样会空转。
     * 轮询读走不会漏样本，实现反而简单。
     */
    if (srv->source_fd > 0) {
        evl_del_fd(srv->loop, srv->source_fd);
        close(srv->source_fd);
    }
    srv->source_fd = fd;
    sample_reader_init(&srv->reader);
    srv->have_sample = false;
    if (srv->source_is_stream) {
        if (evl_add_fd(srv->loop, fd, EVL_READ, on_source_readable, srv) < 0) {
            logmsg("登记数据源失败: %s", strerror(errno));
        }
    }
    return 0;
}

static void source_consume(struct imud *srv, const unsigned char *data,
                           size_t n) {
    struct imu_sample samples[SAMPLE_BATCH];
    size_t got = sample_reader_feed(&srv->reader, data, n, samples,
                                    SAMPLE_BATCH);

    for (size_t i = 0; i < got; i++) {
        const struct imu_sample *s = &samples[i];
        double dt = 1.0 / (double)srv->cfg.sample_rate;

        if (!srv->have_sample) {
            attitude_init(&srv->attitude, s);
            srv->have_sample = true;
        } else if (s->timestamp_ns > srv->last_sample.timestamp_ns) {
            /* 用驱动的时间戳算间隔，比假设采样率准 */
            dt = (double)(s->timestamp_ns - srv->last_sample.timestamp_ns) /
                 1e9;
            attitude_update(&srv->attitude, s, dt, srv->cfg.alpha);
        }
        srv->last_sample = *s;
        srv->pitch_sum += srv->attitude.pitch;
        srv->roll_sum += srv->attitude.roll;
        srv->period_samples++;
    }
}

static void on_source_readable(evl_loop *loop, int fd, unsigned events,
                               void *arg) {
    struct imud *srv = arg;
    unsigned char buf[1024];

    (void)loop;
    (void)events;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);

        if (n > 0) {
            source_consume(srv, buf, (size_t)n);
            continue;
        }
        if (n == 0) {
            /* 命名管道的写端关了：描述符留着，等下一批数据 */
            return;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        logmsg("读 %s 失败: %s", srv->cfg.source, strerror(errno));
        return;
    }
}

/*
 * 定时器驱动的读取：字符设备与普通文件都走这里。
 * 每次把描述符读到 EAGAIN 为止，驱动的 kfifo 因此不会积压。
 */
static void sample_once(struct imud *srv) {
    unsigned char buf[1024];

    for (;;) {
        ssize_t n = read(srv->source_fd, buf, sizeof buf);

        if (n > 0) {
            source_consume(srv, buf, (size_t)n);
            continue;
        }
        return;
    }
}

/* 每 50 ms 把非流式数据源读空：100 Hz 采样下每次只有五六个样本，远小于驱动的队列 */
static void on_pump(evl_loop *loop, evl_timer *timer, void *arg) {
    struct imud *srv = arg;

    (void)loop;
    (void)timer;
    if (!srv->source_is_stream && srv->source_fd > 0) {
        sample_once(srv);
    }
}

/* ---------- 聚合与发布 ---------- */

static void on_report(evl_loop *loop, evl_timer *timer, void *arg) {
    struct imud *srv = arg;
    struct imu_sample sample = { 0 };

    (void)loop;
    (void)timer;
    if (srv->period_samples == 0) {
        /* 这一周期没有新样本：不发，避免上报陈旧值或除零 */
        return;
    }

    /* 一次上报用这一周期内的平均姿态，加速度取最后一个样本 */
    sample = srv->last_sample;
    srv->attitude.pitch = srv->pitch_sum / (double)srv->period_samples;
    srv->attitude.roll = srv->roll_sum / (double)srv->period_samples;
    srv->period_samples = 0;
    srv->pitch_sum = 0.0;
    srv->roll_sum = 0.0;

    if (!srv->mqtt.connected) {
        return;
    }
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    if (mqtt_client_publish_telemetry(&srv->mqtt, (int64_t)ts.tv_sec, &sample,
                                      &srv->attitude, srv->seq++) < 0) {
        logmsg("发布失败，等待重连");
        reconnect_schedule(srv, srv->reconnect_delay_ms);
    }
}

/* ---------- MQTT 连接维护 ---------- */

static void reconnect_schedule(struct imud *srv, int delay_ms) {
    /*
     * evl 的一次性定时器在回调返回后就被释放，句柄随即失效。
     * 所以回调里先把句柄清空，这里只在句柄还有效时才取消。
     */
    if (srv->reconnect_timer != NULL) {
        evl_cancel_timer(srv->loop, srv->reconnect_timer);
        srv->reconnect_timer = NULL;
    }
    srv->reconnect_timer = evl_add_timer(srv->loop, delay_ms, 0,
                                         on_reconnect, srv);
}

static void on_reconnect(evl_loop *loop, evl_timer *timer, void *arg) {
    struct imud *srv = arg;

    (void)loop;
    (void)timer;
    srv->reconnect_timer = NULL; /* 这个一次性定时器马上会被 evl 释放 */
    if (mqtt_client_connect(&srv->mqtt, &srv->cfg) == 0) {
        srv->reconnect_delay_ms = RECONNECT_MIN_MS;
        return;
    }
    srv->reconnect_delay_ms *= 2;
    if (srv->reconnect_delay_ms > RECONNECT_MAX_MS) {
        srv->reconnect_delay_ms = RECONNECT_MAX_MS;
    }
    logmsg("%d 毫秒后重试", srv->reconnect_delay_ms);
    reconnect_schedule(srv, srv->reconnect_delay_ms);
}

static void on_poll(evl_loop *loop, evl_timer *timer, void *arg) {
    struct imud *srv = arg;

    (void)loop;
    (void)timer;
    if (!srv->mqtt.connected) {
        return;
    }
    if (mqtt_client_poll(&srv->mqtt) < 0) {
        mqtt_client_disconnect(&srv->mqtt, false);
        logmsg("连接断开，%d 毫秒后重连", srv->reconnect_delay_ms);
        reconnect_schedule(srv, srv->reconnect_delay_ms);
        return;
    }
    if (srv->mqtt.cmd_rate_pending) {
        srv->mqtt.cmd_rate_pending = false;
        logmsg("收到 rate=%d 命令", srv->mqtt.cmd_rate_value);
        srv->cfg.sample_rate = srv->mqtt.cmd_rate_value;
        /* 采样率属于驱动，经 sysfs 下发；失败只记日志 */
        char path[256];
        FILE *fp;

        snprintf(path, sizeof path, "/sys/class/misc/mdr_imu/sample_rate");
        fp = fopen(path, "w");
        if (fp != NULL) {
            fprintf(fp, "%d\n", srv->cfg.sample_rate);
            fclose(fp);
            logmsg("采样率已设为 %d Hz", srv->cfg.sample_rate);
        } else {
            logmsg("写 %s 失败: %s", path, strerror(errno));
        }
    }
}

/* ---------- 配置重载 ---------- */

static void on_signal(evl_loop *loop, int fd, unsigned events, void *arg) {
    struct imud *srv = arg;
    struct signalfd_siginfo info;

    (void)events;
    while (read(fd, &info, sizeof info) == sizeof info) {
        if (info.ssi_signo == SIGTERM || info.ssi_signo == SIGINT) {
            logmsg("收到信号 %u，退出", info.ssi_signo);
            evl_stop(loop);
        } else if (info.ssi_signo == SIGHUP) {
            struct imud_config next = srv->cfg;

            if (config_load(srv->conf_path, &next) < 0) {
                logmsg("重载失败，继续使用旧配置");
                continue;
            }
            if (strcmp(next.source, srv->cfg.source) != 0 &&
                source_open(srv, next.source) < 0) {
                logmsg("换数据源失败，继续使用旧配置");
                continue;
            }
            if (next.period_ms != srv->cfg.period_ms) {
                evl_cancel_timer(srv->loop, srv->report_timer);
                srv->report_timer = evl_add_timer(loop, next.period_ms,
                                                  next.period_ms, on_report,
                                                  srv);
            }
            srv->cfg = next;
            logmsg("重载配置: source=%s period_ms=%d", srv->cfg.source,
                   srv->cfg.period_ms);
        }
    }
}

/* ---------- main ---------- */

static void usage(const char *prog) {
    fprintf(stderr, "用法: %s [-f 配置文件] [-s 数据源]\n", prog);
    fprintf(stderr, "  -f  配置文件，缺省 ./imud.conf\n");
    fprintf(stderr, "  -s  覆盖配置里的数据源，调试时可指向命名管道\n");
}

int main(int argc, char *argv[]) {
    struct imud *srv;
    const char *conf = "imud.conf";
    const char *override_source = NULL;
    int opt;
    int sigfd;
    sigset_t mask;

    while ((opt = getopt(argc, argv, "f:s:h")) != -1) {
        switch (opt) {
        case 'f':
            conf = optarg;
            break;
        case 's':
            override_source = optarg;
            break;
        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    srv = calloc(1, sizeof *srv);
    if (srv == NULL) {
        return EXIT_FAILURE;
    }
    config_defaults(&srv->cfg);
    if (config_load(conf, &srv->cfg) < 0) {
        logmsg("读配置 %s 失败，使用缺省值", conf);
    }
    if (override_source != NULL) {
        snprintf(srv->cfg.source, sizeof srv->cfg.source, "%s",
                 override_source);
    }
    srv->conf_path = conf;
    srv->reconnect_delay_ms = RECONNECT_MIN_MS;
    srv->source_fd = -1;

    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        perror("sigprocmask");
        free(srv);
        return EXIT_FAILURE;
    }
    sigfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (sigfd < 0) {
        perror("signalfd");
        free(srv);
        return EXIT_FAILURE;
    }

    srv->loop = evl_create();
    if (srv->loop == NULL) {
        perror("evl_create");
        return EXIT_FAILURE;
    }
    sample_reader_init(&srv->reader);
    if (source_open(srv, srv->cfg.source) < 0) {
        logmsg("打不开数据源 %s，仍会启动并等待配置重载",
               srv->cfg.source);
        srv->source_fd = -1;
    }

    mqtt_client_init(&srv->mqtt, &srv->cfg);
    if (mqtt_client_connect(&srv->mqtt, &srv->cfg) < 0) {
        logmsg("首次连接失败，进入重连循环");
        reconnect_schedule(srv, srv->reconnect_delay_ms);
    } else {
        srv->reconnect_delay_ms = RECONNECT_MIN_MS;
    }

    evl_add_fd(srv->loop, sigfd, EVL_READ, on_signal, srv);
    srv->report_timer = evl_add_timer(srv->loop, srv->cfg.period_ms,
                                      srv->cfg.period_ms, on_report, srv);
    srv->poll_timer = evl_add_timer(srv->loop, MQTT_POLL_MS, MQTT_POLL_MS,
                                    on_poll, srv);
    srv->pump_timer = evl_add_timer(srv->loop, PUMP_MS, PUMP_MS, on_pump,
                                    srv);

    logmsg("imud 启动: pid=%ld source=%s period_ms=%d", (long)getpid(),
           srv->cfg.source, srv->cfg.period_ms);

    evl_run(srv->loop);

    mqtt_client_disconnect(&srv->mqtt, true);
    if (srv->source_fd > 0) {
        close(srv->source_fd);
    }
    close(sigfd);
    evl_destroy(srv->loop);
    free(srv);
    return EXIT_SUCCESS;
}