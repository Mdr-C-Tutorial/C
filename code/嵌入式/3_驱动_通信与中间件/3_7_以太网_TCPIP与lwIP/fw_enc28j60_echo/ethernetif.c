#include "ethernetif.h"
#include "enc28j60.h"
#include "lwip/etharp.h"
#include "lwip/pbuf.h"

static void low_level_init(struct netif *netif) {
    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif->hwaddr[0] = 0x02u;
    netif->hwaddr[1] = 0x00u;
    netif->hwaddr[2] = 0x00u;
    netif->hwaddr[3] = 0x10u;
    netif->hwaddr[4] = 0x3Cu;
    netif->hwaddr[5] = 0x80u;

    netif->mtu = 1500u;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    enc28j60_init(netif->hwaddr);
}

static err_t low_level_output(struct netif *netif, struct pbuf *p) {
    (void)netif;
    enc28j60_begin_tx();
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        enc28j60_write_buf(q->payload, q->len);
    }
    enc28j60_end_tx(p->tot_len);
    return ERR_OK;
}

static struct pbuf *low_level_input(struct netif *netif) {
    (void)netif;
    if (enc28j60_pending_packets() == 0u) {
        return NULL;
    }

    uint16_t len = enc28j60_begin_rx();
    if (len == 0u) {
        enc28j60_end_rx();
        return NULL;
    }

    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p != NULL) {
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            enc28j60_read_buf(q->payload, q->len);
        }
    }
    enc28j60_end_rx();
    return p;
}

void ethernetif_poll(struct netif *netif) {
    struct pbuf *p = low_level_input(netif);
    if (p != NULL) {
        if (netif->input(p, netif) != ERR_OK) {
            pbuf_free(p);
        }
    }
}

err_t ethernetif_init(struct netif *netif) {
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;
    low_level_init(netif);
    return ERR_OK;
}
