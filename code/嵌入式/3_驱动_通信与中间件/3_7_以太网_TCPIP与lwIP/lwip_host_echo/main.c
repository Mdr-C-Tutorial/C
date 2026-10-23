#include "echo_server.h"
#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include <stdio.h>
#include <string.h>

static u32_t g_sim_ms = 0u;
static int g_client_done = 0;

/* 为 lwIP 提供单调递增的毫秒节拍 */
u32_t sys_now(void) {
    return g_sim_ms;
}

static err_t client_recv_cb(void *arg, struct tcp_pcb *tpcb,
                            struct pbuf *p, err_t err) {
    (void)arg;
    if (p == NULL) {
        printf("[client] server closed connection\n");
        g_client_done = 1;
        return ERR_OK;
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    char buf[128];
    u16_t copied = pbuf_copy_partial(p, buf, sizeof(buf) - 1u, 0u);
    buf[copied] = '\0';
    printf("[client] echoed (%u bytes): %s", (unsigned)copied, buf);

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    /* 收到回显后主动关闭连接 */
    tcp_close(tpcb);
    g_client_done = 1;
    return ERR_OK;
}

static err_t client_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        return err;
    }
    const char *msg = "hello, lwip\n";
    printf("[client] connected, sending %u bytes: %s",
           (unsigned)strlen(msg), msg);
    tcp_recv(tpcb, client_recv_cb);
    err_t wr = tcp_write(tpcb, msg, (u16_t)strlen(msg), TCP_WRITE_FLAG_COPY);
    if (wr != ERR_OK) {
        return wr;
    }
    return tcp_output(tpcb);
}

int main(void) {
    lwip_init();
    if (echo_server_init() != ERR_OK) {
        printf("Failed to init echo server\n");
        return 1;
    }

    struct tcp_pcb *cli = tcp_new_ip_type(IPADDR_TYPE_V4);
    ip_addr_t dst;
    IP_ADDR4(&dst, 127, 0, 0, 1);
    tcp_connect(cli, &dst, 7, client_connected_cb);

    /* NO_SYS 主循环：轮询回环接口并检查协议栈定时器 */
    for (int step = 0; step < 200 && !g_client_done; ++step) {
        g_sim_ms += 10u;
        netif_poll_all();
        sys_check_timeouts();
    }
    /* 再推进几轮使 FIN/ACK 四次挥手完成 */
    for (int step = 0; step < 20; ++step) {
        g_sim_ms += 10u;
        netif_poll_all();
        sys_check_timeouts();
    }

    printf("[main] test finished, client_done=%d\n", g_client_done);
    return g_client_done ? 0 : 1;
}
