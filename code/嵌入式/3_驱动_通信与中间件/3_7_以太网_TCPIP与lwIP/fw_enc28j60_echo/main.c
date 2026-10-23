#include "clock.h"
#include "console.h"
#include "echo_server.h"
#include "ethernetif.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "tick.h"
#include <stdint.h>

static struct netif g_netif;
static uint32_t g_rng_state = 0x13579BDFu;

uint32_t lwip_port_rand(void) {
    g_rng_state = g_rng_state * 1664525u + 1013904223u;
    return g_rng_state;
}

/* 由 2.6 节的 SysTick 毫秒节拍直接提供 lwIP 时钟源 */
u32_t sys_now(void) {
    return (u32_t)tick_now();
}

static void netif_status_cb(struct netif *netif) {
    if (netif_is_up(netif) && !ip4_addr_isany_val(*netif_ip4_addr(netif))) {
        uart_puts("netif IP: ");
        uart_puts(ip4addr_ntoa(netif_ip4_addr(netif)));
        uart_puts("\r\n");
    }
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);

    lwip_init();

    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;
    IP4_ADDR(&ipaddr, 192, 168, 1, 50);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);

    netif_add(&g_netif, &ipaddr, &netmask, &gw,
              NULL, ethernetif_init, ethernet_input);
    netif_set_status_callback(&g_netif, netif_status_cb);
    netif_set_default(&g_netif);
    netif_set_up(&g_netif);

    /* 若需要通过路由器动态获取 IP，先将静态地址置 0 再调用 dhcp_start(&g_netif) */
    (void)echo_server_init();

    for (;;) {
        ethernetif_poll(&g_netif);
        sys_check_timeouts();
    }
}
