/*
 * uart_device_sample.c：用 rt_device 的接收回调 + 信号量读串口。
 *
 * 接收回调在中断上下文里执行，只能做最少的事：释放信号量。
 * 读取与解析放在线程里，这是 RT-Thread 设备框架推荐的用法。
 */
#include <rtthread.h>
#include <rtdevice.h>

#define CONSOLE_DEVICE "uart1"
#define READ_BUF_SIZE  32

static struct rt_semaphore g_rx_sem;
static rt_device_t g_uart;

/* 接收回调：串口每收到一批字节就调一次，运行在中断上下文 */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size) {
    (void)dev;
    if (size > 0) {
        rt_sem_release(&g_rx_sem);
    }
    return RT_EOK;
}

static void reader_thread(void *param) {
    char buf[READ_BUF_SIZE];

    (void)param;
    for (;;) {
        rt_size_t n;

        if (rt_sem_take(&g_rx_sem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }
        n = rt_device_read(g_uart, -1, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            rt_kprintf("rx: %s\n", buf);
        }
    }
}

static int uart_sample_init(void) {
    rt_thread_t tid;

    g_uart = rt_device_find(CONSOLE_DEVICE);
    if (g_uart == RT_NULL) {
        return -RT_ERROR;
    }
    if (rt_device_open(g_uart, RT_DEVICE_FLAG_INT_RX) != RT_EOK) {
        return -RT_ERROR;
    }
    rt_device_set_rx_indicate(g_uart, uart_rx_ind);
    rt_sem_init(&g_rx_sem, "rx", 0, RT_IPC_FLAG_PRIO);

    tid = rt_thread_create("reader", reader_thread, RT_NULL, 1024, 12, 10);
    if (tid != RT_NULL) {
        rt_thread_startup(tid);
    }
    return RT_EOK;
}
INIT_APP_EXPORT(uart_sample_init);