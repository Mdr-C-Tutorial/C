#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint16_t g_holding_regs[16] = { 100u, 200u, 300u, 400u };
static u32_t g_sim_ms = 0u;
static int g_replies = 0;

u32_t sys_now(void) {
    return g_sim_ms;
}

/*
 * TODO：解析一帧 Modbus TCP 请求 req（长度 req_len），写入应答 resp 并返回应答字节数：
 * - MBAP 首部 7 字节：事务 ID (0..1)、协议 ID (2..3，须为 0x0000)、
 *   后续长度 (4..5)、单元标识 (6)；
 * - PDU 从 req[7] 开始：功能码 0x03（读保持寄存器）或 0x06（写单个寄存器），
 *   寄存器地址与数值均为大端序；
 * - 应答帧复制事务 ID、协议 ID 0x0000、单元标识，并在长度字段填入 1 + 应答 PDU 长度。
 */
size_t modbus_tcp_handle(const uint8_t *req, size_t req_len, uint8_t *resp) {
    (void)req;
    (void)req_len;
    (void)resp;
    return 0u;
}

static err_t mb_recv_cb(void *arg, struct tcp_pcb *tpcb,
                        struct pbuf *p, err_t err) {
    (void)arg;
    if (p == NULL) {
        return tcp_close(tpcb);
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    uint8_t req[64];
    uint8_t resp[64];
    u16_t len = pbuf_copy_partial(p, req, sizeof(req), 0u);
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    size_t resp_len = modbus_tcp_handle(req, len, resp);
    if (resp_len > 0u) {
        tcp_write(tpcb, resp, (u16_t)resp_len, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    }
    return ERR_OK;
}

static err_t mb_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || newpcb == NULL) {
        return ERR_VAL;
    }
    tcp_recv(newpcb, mb_recv_cb);
    return ERR_OK;
}

static err_t cli_recv_cb(void *arg, struct tcp_pcb *tpcb,
                         struct pbuf *p, err_t err) {
    (void)arg;
    if (p == NULL || err != ERR_OK) {
        if (p != NULL) {
            pbuf_free(p);
        }
        return ERR_OK;
    }
    uint8_t buf[64];
    u16_t n = pbuf_copy_partial(p, buf, sizeof(buf), 0u);
    printf("[client] rx (%u bytes):", (unsigned)n);
    for (u16_t i = 0u; i < n; ++i) {
        printf(" %02X", buf[i]);
    }
    printf("\n");
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    ++g_replies;
    return ERR_OK;
}

static err_t cli_conn_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
    (void)arg;
    if (err == ERR_OK) {
        tcp_recv(tpcb, cli_recv_cb);
    }
    return err;
}

int main(void) {
    lwip_init();

    struct tcp_pcb *srv = tcp_new_ip_type(IPADDR_TYPE_ANY);
    tcp_bind(srv, IP_ANY_TYPE, 502);
    srv = tcp_listen(srv);
    tcp_accept(srv, mb_accept_cb);

    struct tcp_pcb *cli = tcp_new_ip_type(IPADDR_TYPE_V4);
    ip_addr_t loop_ip;
    IP_ADDR4(&loop_ip, 127, 0, 0, 1);
    tcp_connect(cli, &loop_ip, 502, cli_conn_cb);

    for (int i = 0; i < 10; ++i) {
        g_sim_ms += 10u;
        netif_poll_all();
        sys_check_timeouts();
    }

    /* 1. 功能码 0x06：把保持寄存器 0x0001 写为 0x1234（事务 ID = 0x0001） */
    const uint8_t req_write[12] = {
        0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x01,
        0x06, 0x00, 0x01, 0x12, 0x34
    };
    tcp_write(cli, req_write, sizeof(req_write), TCP_WRITE_FLAG_COPY);
    tcp_output(cli);
    for (int i = 0; i < 10; ++i) {
        g_sim_ms += 10u;
        netif_poll_all();
        sys_check_timeouts();
    }

    /* 2. 功能码 0x03：读取保持寄存器 0x0000～0x0001 共 2 个（事务 ID = 0x0002） */
    const uint8_t req_read[12] = {
        0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x01,
        0x03, 0x00, 0x00, 0x00, 0x02
    };
    tcp_write(cli, req_read, sizeof(req_read), TCP_WRITE_FLAG_COPY);
    tcp_output(cli);
    for (int i = 0; i < 10; ++i) {
        g_sim_ms += 10u;
        netif_poll_all();
        sys_check_timeouts();
    }

    return g_replies == 2 ? 0 : 1;
}
