#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include <stdio.h>
#include <string.h>

#define LINE_MAX_LEN 64u

struct line_state {
    char buf[LINE_MAX_LEN];
    size_t len;
};

static struct line_state g_conn_state;
static u32_t g_sim_ms = 0u;
static int g_rx_lines = 0;

u32_t sys_now(void) {
    return g_sim_ms;
}

/*
 * TODO：在接收回调中逐字节遍历 pbuf 链：
 * 1. 将字节追加到 st->buf，若未遇到 '\n' 且已满 LINE_MAX_LEN，则丢弃缓冲并发送
 *    "ERR: line too long\n"；
 * 2. 每当遇到 '\n' 时，发送 "ECHO: " + 该行内容（含 '\n'），然后清空 st->len；
 * 3. 调用 tcp_recved(tpcb, p->tot_len)、pbuf_free(p) 与 tcp_output(tpcb)。
 */
static err_t line_recv_cb(void *arg, struct tcp_pcb *tpcb,
                          struct pbuf *p, err_t err) {
    struct line_state *st = (struct line_state *)arg;
    (void)st;
    if (p == NULL) {
        return tcp_close(tpcb);
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t line_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || newpcb == NULL) {
        return ERR_VAL;
    }
    memset(&g_conn_state, 0, sizeof(g_conn_state));
    tcp_arg(newpcb, &g_conn_state);
    tcp_recv(newpcb, line_recv_cb);
    return ERR_OK;
}

static err_t client_recv_cb(void *arg, struct tcp_pcb *tpcb,
                            struct pbuf *p, err_t err) {
    (void)arg;
    if (p == NULL || err != ERR_OK) {
        if (p != NULL) {
            pbuf_free(p);
        }
        return ERR_OK;
    }
    char buf[128];
    u16_t n = pbuf_copy_partial(p, buf, sizeof(buf) - 1u, 0u);
    buf[n] = '\0';
    printf("[client] recv: %s", buf);
    for (u16_t i = 0u; i < n; ++i) {
        if (buf[i] == '\n') {
            ++g_rx_lines;
        }
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t client_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        return err;
    }
    tcp_recv(tpcb, client_recv_cb);
    return ERR_OK;
}

int main(void) {
    lwip_init();

    struct tcp_pcb *srv = tcp_new_ip_type(IPADDR_TYPE_ANY);
    tcp_bind(srv, IP_ANY_TYPE, 7007);
    srv = tcp_listen(srv);
    tcp_accept(srv, line_accept_cb);

    struct tcp_pcb *cli = tcp_new_ip_type(IPADDR_TYPE_V4);
    ip_addr_t loop_ip;
    IP_ADDR4(&loop_ip, 127, 0, 0, 1);
    tcp_connect(cli, &loop_ip, 7007, client_connected_cb);

    for (int i = 0; i < 10; ++i) {
        g_sim_ms += 10u;
        netif_poll_all();
        sys_check_timeouts();
    }

    const char *chunks[3] = { "hel", "lo\nwor", "ld\n" };
    for (int c = 0; c < 3; ++c) {
        tcp_write(cli, chunks[c], (u16_t)strlen(chunks[c]), TCP_WRITE_FLAG_COPY);
        tcp_output(cli);
        for (int i = 0; i < 10; ++i) {
            g_sim_ms += 10u;
            netif_poll_all();
            sys_check_timeouts();
        }
    }

    printf("Total echoed lines: %d\n", g_rx_lines);
    return 0;
}
