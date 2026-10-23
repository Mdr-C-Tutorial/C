/*
 * producer_consumer_rtt.c：4.3 节 producer_consumer 的 RT-Thread 版。
 *
 * 两个线程：sampler 每 10 ms 采一次并投进消息队列，proc 取出来做四点滑动平均。
 * 与 FreeRTOS 版的区别在参数与约定：优先级 10 比 12 高（RT-Thread 里 0 最高），
 * 栈大小以字节计（1024 字节），rt_mq_recv 的返回值是收到的字节数。
 *
 * 编译检查（需要 RT-Thread v5.3.0 源码）：
 *   arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -std=gnu11 -O2 -Wall -Wextra \
 *       -ffreestanding -D__RTTHREAD__ -I<rt-thread>/include \
 *       -I<rt-thread>/libcpu/arm/cortex-m3 -c producer_consumer_rtt.c
 */
#include <stdarg.h>

#include <rtthread.h>

#define SAMPLE_COUNT 6
#define CONSOLE_BUF_SIZE 128

struct sample {
    rt_tick_t tick;
    rt_int16_t ax;
    rt_int16_t ay;
    rt_int16_t az;
};

static const rt_int16_t g_table[][3] = {
    { 12, -18,  998 },
    { 16, -14, 1006 },
    { 10, -20, 1002 },
    { 14, -16,  994 },
    { 18, -12, 1010 },
    {  8, -22,  990 },
    { 15, -15, 1004 },
    { 11, -19,  996 },
};

static rt_mq_t g_sample_mq;
static rt_mutex_t g_console_lock;

static void fake_sensor_read(int index, struct sample *out) {
    int n = (int)(sizeof(g_table) / sizeof(g_table[0]));
    int i = index % n;

    out->tick = rt_tick_get();
    out->ax = g_table[i][0];
    out->ay = g_table[i][1];
    out->az = g_table[i][2];
}

/* 先格式化到局部缓冲区，再加锁输出，锁的持有时间尽量短 */
static void log_line(const char *fmt, ...) {
    char buf[CONSOLE_BUF_SIZE];
    va_list args;

    va_start(args, fmt);
    rt_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    rt_mutex_take(g_console_lock, RT_WAITING_FOREVER);
    rt_kprintf("%s", buf);
    rt_mutex_release(g_console_lock);
}

static void sampler_thread(void *param) {
    (void)param;

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        struct sample s;

        fake_sensor_read(i, &s);
        if (rt_mq_send(g_sample_mq, &s, sizeof(s)) == RT_EOK) {
            log_line("[sampler]   sent #%d (az=%d mg)\n", i + 1, s.az);
        }
        rt_thread_mdelay(10);
    }
}

static void processor_thread(void *param) {
    rt_int32_t window[4] = { 0 };
    int count = 0;

    (void)param;
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        struct sample s;

        if (rt_mq_recv(g_sample_mq, &s, sizeof(s), RT_WAITING_FOREVER) > 0) {
            rt_int32_t sum = 0;
            int valid;

            window[count % 4] = s.az;
            count++;
            valid = count < 4 ? count : 4;
            for (int k = 0; k < valid; ++k) {
                sum += window[k];
            }
            log_line("[processor] recv #%d: ax=%d ay=%d az=%d -> avg_az=%d mg\n",
                     count, s.ax, s.ay, s.az, (int)(sum / valid));
        }
    }
}

/* 用自动初始化代替 main 里逐个建线程：启动到这一级时调度器已经在跑 */
static int producer_consumer_start(void) {
    rt_thread_t tid;

    g_sample_mq = rt_mq_create("sample", sizeof(struct sample), 4, RT_IPC_FLAG_PRIO);
    if (g_sample_mq == RT_NULL) {
        return -RT_ERROR;
    }
    g_console_lock = rt_mutex_create("console", RT_IPC_FLAG_PRIO);
    if (g_console_lock == RT_NULL) {
        return -RT_ERROR;
    }

    tid = rt_thread_create("sampler", sampler_thread, RT_NULL, 1024, 10, 10);
    if (tid != RT_NULL) {
        rt_thread_startup(tid);
    }
    tid = rt_thread_create("proc", processor_thread, RT_NULL, 1024, 12, 10);
    if (tid != RT_NULL) {
        rt_thread_startup(tid);
    }
    return RT_EOK;
}
INIT_APP_EXPORT(producer_consumer_start);

int main(void) {
    return 0;
}