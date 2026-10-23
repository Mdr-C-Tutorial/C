#include "echo_server.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

static void echo_err_cb(void *arg, err_t err) {
    (void)arg;
    (void)err;
}

static err_t echo_recv_cb(void *arg, struct tcp_pcb *tpcb,
                          struct pbuf *p, err_t err) {
    (void)arg;
    if (p == NULL) {
        tcp_arg(tpcb, NULL);
        tcp_recv(tpcb, NULL);
        tcp_err(tpcb, NULL);
        return tcp_close(tpcb);
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    for (struct pbuf *q = p; q != NULL; q = q->next) {
        u8_t flags = TCP_WRITE_FLAG_COPY;
        if (q->next != NULL) {
            flags |= TCP_WRITE_FLAG_MORE;
        }
        err_t wr_err = tcp_write(tpcb, q->payload, q->len, flags);
        if (wr_err != ERR_OK) {
            return wr_err;
        }
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return tcp_output(tpcb);
}

static err_t echo_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || newpcb == NULL) {
        return ERR_VAL;
    }
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
    return ERR_OK;
}
