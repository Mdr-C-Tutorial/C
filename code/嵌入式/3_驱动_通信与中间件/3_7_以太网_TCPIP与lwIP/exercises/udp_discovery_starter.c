#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
#include <stdio.h>
#include <string.h>

static u32_t g_sim_ms = 0u;
static int g_got_reply = 0;

u32_t sys_now(void) {
    return g_sim_ms;
}

/*
 * TODO：UDP 接收回调：
 * 1. 检查 p->tot_len == 4 且内容为 "WHO?"；
 * 2. 若匹配，用 pbuf_alloc(PBUF_TRANSPORT, reply_len, PBUF_RAM) 分配响应 pbuf，
 *    填入 "DEV=F103-NODE;IP=127.0.0.1"，调用 udp_sendto(upcb, rep, addr, port)
 *    并释放 rep；
 * 3. 无论是否匹配，最后都要调用 pbuf_free(p) 释放收到的 pbuf。
 */
static void discovery_recv_cb(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                              const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)upcb;
    (void)addr;
    (void)port;
    if (p != NULL) {
        pbuf_free(p);
    }
}

static void client_udp_recv_cb(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                               const ip_addr_t *addr, u16_t port) {
    (void)arg;
    (void)upcb;
    if (p == NULL) {
        return;
    }
    char buf[64];
    u16_t n = pbuf_copy_partial(p, buf, sizeof(buf) - 1u, 0u);
    buf[n] = '\0';
    printf("[client] reply from %s:%u -> %s\n",
           ipaddr_ntoa(addr), (unsigned)port, buf);
    pbuf_free(p);
    g_got_reply = 1;
}

int main(void) {
    lwip_init();

    struct udp_pcb *srv = udp_new_ip_type(IPADDR_TYPE_ANY);
    udp_bind(srv, IP_ANY_TYPE, 9999);
    udp_recv(srv, discovery_recv_cb, NULL);

    struct udp_pcb *cli = udp_new_ip_type(IPADDR_TYPE_V4);
    udp_bind(cli, IP_ANY_TYPE, 10000);
    udp_recv(cli, client_udp_recv_cb, NULL);

    ip_addr_t dst;
    IP_ADDR4(&dst, 127, 0, 0, 1);

    const char *q = "WHO?";
    struct pbuf *qp = pbuf_alloc(PBUF_TRANSPORT, (u16_t)strlen(q), PBUF_RAM);
    memcpy(qp->payload, q, strlen(q));
    udp_sendto(cli, qp, &dst, 9999);
    pbuf_free(qp);

    for (int i = 0; i < 20 && !g_got_reply; ++i) {
        g_sim_ms += 10u;
        netif_poll_all();
        sys_check_timeouts();
    }
    return g_got_reply ? 0 : 1;
}
