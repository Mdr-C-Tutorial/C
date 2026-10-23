#include "echo_server.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include <stdio.h>

static void echo_err_cb(void *arg, err_t err) {
    (void)arg;
    printf("[server] connection error: %d\n", (int)err);
}

static err_t echo_recv_cb(void *arg, struct tcp_pcb *tpcb,
                          struct pbuf *p, err_t err) {
    (void)arg;
    if (p == NULL) {
        /* 对端已发送 FIN 关闭连接 */
        printf("[server] peer closed connection\n");
        tcp_arg(tpcb, NULL);
        tcp_recv(tpcb, NULL);
        tcp_err(tpcb, NULL);
        return tcp_close(tpcb);
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    printf("[server] recv tot_len=%u bytes\n", (unsigned)p->tot_len);

    /* 遍历 pbuf 链，将每一段载荷写入发送队列 */
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        u8_t flags = TCP_WRITE_FLAG_COPY;
        if (q->next != NULL) {
            flags |= TCP_WRITE_FLAG_MORE;
        }
        err_t wr_err = tcp_write(tpcb, q->payload, q->len, flags);
        if (wr_err != ERR_OK) {
            /* 发送缓冲区不足时返回 ERR_MEM，保留 pbuf 留待下次处理 */
            return wr_err;
        }
    }

    /* 通知协议栈应用层已消费 p->tot_len 字节，推进接收窗口 */
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    /* 立即把发送队列中的报文段发往网络层 */
    return tcp_output(tpcb);
}

static err_t echo_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || newpcb == NULL) {
        return ERR_VAL;
    }
    printf("[server] accepted connection from %s:%u\n",
           ipaddr_ntoa(&newpcb->remote_ip), (unsigned)newpcb->remote_port);
    tcp_recv(newpcb, echo_recv_cb);
    tcp_err(newpcb, echo_err_cb);
    return ERR_OK;
}

err_t echo_server_init(void) {
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == NULL) {
        return ERR_MEM;
    }
    err_t err = tcp_bind(pcb, IP_ANY_TYPE, 7);
    if (err != ERR_OK) {
        tcp_close(pcb);
        return err;
    }
    struct tcp_pcb *lpcb = tcp_listen(pcb);
    if (lpcb == NULL) {
        tcp_close(pcb);
        return ERR_MEM;
    }
    tcp_accept(lpcb, echo_accept_cb);
    printf("[server] listening on port 7\n");
    return ERR_OK;
}
