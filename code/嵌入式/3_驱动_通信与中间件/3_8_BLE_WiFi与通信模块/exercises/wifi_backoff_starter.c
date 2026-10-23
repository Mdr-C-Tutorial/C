/*
 * 习题 60382 初始代码：断线后按 1、2、4、8 秒指数退避（上限 30 秒）重连 Wi-Fi。
 *
 * 编译：
 *   gcc -std=c17 -Wall -Wextra exercises/wifi_backoff_starter.c -o wifi_backoff_starter
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct wifi_reconnect {
    bool connected;
    bool waiting_retry;
    uint32_t next_delay_ms;
    uint32_t disconnect_ms;
};

static void wifi_reconnect_init(struct wifi_reconnect *r) {
    r->connected = true;
    r->waiting_retry = false;
    r->next_delay_ms = 1000u;
    r->disconnect_ms = 0u;
}

static void wifi_on_disconnect(struct wifi_reconnect *r, uint32_t now_ms) {
    r->connected = false;
    r->waiting_retry = true;
    r->disconnect_ms = now_ms;
}

static void wifi_on_got_ip(struct wifi_reconnect *r) {
    r->connected = true;
    r->waiting_retry = false;
    r->next_delay_ms = 1000u;
}

static void wifi_poll(struct wifi_reconnect *r, uint32_t now_ms) {
    if (!r->waiting_retry) {
        return;
    }
    if ((uint32_t)(now_ms - r->disconnect_ms) >= r->next_delay_ms) {
        printf("[t=%5u ms] send AT+CWJAP (backoff=%u ms)\n",
               now_ms, r->next_delay_ms);
        r->disconnect_ms = now_ms;
        uint32_t doubled = r->next_delay_ms * 2u;
        r->next_delay_ms = (doubled > 30000u) ? 30000u : doubled;
    }
}

int main(void) {
    struct wifi_reconnect mgr;
    wifi_reconnect_init(&mgr);

    wifi_on_disconnect(&mgr, 0u);
    for (uint32_t t = 0u; t <= 16000u; t += 500u) {
        wifi_poll(&mgr, t);
    }

    wifi_on_got_ip(&mgr);
    wifi_on_disconnect(&mgr, 20000u);
    for (uint32_t t = 20000u; t <= 21500u; t += 500u) {
        wifi_poll(&mgr, t);
    }
    return 0;
}
