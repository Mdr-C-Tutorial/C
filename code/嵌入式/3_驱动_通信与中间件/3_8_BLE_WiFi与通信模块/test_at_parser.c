#include "at_parser.h"
#include "ringbuf.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void mock_uart_write(const uint8_t *data, size_t len, void *user_data) {
    (void)user_data;
    printf("[TX] ");
    for (size_t i = 0u; i < len; ++i) {
        if (data[i] == (uint8_t)'\r') {
            printf("\\r");
        } else if (data[i] == (uint8_t)'\n') {
            printf("\\n");
        } else {
            putchar((int)data[i]);
        }
    }
    putchar('\n');
}

static void on_urc(const char *urc_line, void *user_data) {
    (void)user_data;
    printf("[URC] %s\n", urc_line);
}

static void on_ipd(const uint8_t *data, size_t len, void *user_data) {
    (void)user_data;
    printf("[IPD] len=%u:", (unsigned)len);
    for (size_t i = 0u; i < len; ++i) {
        printf(" %02X", data[i]);
    }
    putchar('\n');
}

static void on_prompt(void *user_data) {
    struct at_client *at = (struct at_client *)user_data;
    const uint8_t payload[] = "hello";
    printf("[PROMPT] got '>', sending 5 bytes\n");
    at->write_fn(payload, 5u, at->io_user);
}

static void on_cmd_done(enum at_result result,
                        const char *info,
                        void *user_data) {
    const char *tag = (const char *)user_data;
    if (info[0] != '\0') {
        printf("[CMD %s] result=%s info=\"%s\"\n",
               tag, at_result_str(result), info);
    } else {
        printf("[CMD %s] result=%s\n", tag, at_result_str(result));
    }
}

static void feed_chunk(struct ringbuf *rb, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    for (size_t i = 0u; i < len; ++i) {
        (void)rb_put(rb, p[i]);
    }
}

int main(void) {
    uint8_t rb_mem[256];
    struct ringbuf rx_rb;
    struct at_client at;

    rb_init(&rx_rb, rb_mem, sizeof(rb_mem));
    at_client_init(&at, &rx_rb, mock_uart_write, NULL);
    at_set_callbacks(&at, on_urc, on_ipd, on_prompt, &at);

    /* 1. 发送联网命令，模拟应答被拆成 3 段且夹杂 URC */
    at_poll(&at, 100u);
    at_send(&at, "AT+CWJAP=\"LabAP\",\"pass1234\"",
            15000u, on_cmd_done, "CWJAP");

    const char frag1[] = "WIFI CON";
    const char frag2[] = "NECTED\r\nWIFI GOT IP\r\n";
    const char frag3[] = "\r\nOK\r\n";
    feed_chunk(&rx_rb, frag1, sizeof(frag1) - 1u);
    at_poll(&at, 200u);
    feed_chunk(&rx_rb, frag2, sizeof(frag2) - 1u);
    at_poll(&at, 300u);
    feed_chunk(&rx_rb, frag3, sizeof(frag3) - 1u);
    at_poll(&at, 400u);

    /* 2. 发送数据命令：模块先回 OK，再给 '>' 提示符，写负载后收 SEND OK */
    at_send(&at, "AT+CIPSEND=5", 2000u, on_cmd_done, "CIPSEND");
    const char prompt_chunk[] = "\r\nOK\r\n>";
    const char send_ok_chunk[] = "\r\nRecv 5 bytes\r\n\r\nSEND OK\r\n";
    feed_chunk(&rx_rb, prompt_chunk, sizeof(prompt_chunk) - 1u);
    at_poll(&at, 450u);
    feed_chunk(&rx_rb, send_ok_chunk, sizeof(send_ok_chunk) - 1u);
    at_poll(&at, 500u);

    /* 3. 接收含 \r\n、0x00 和 "OK" 字节的 +IPD 二进制帧，紧跟 CLOSED */
    const uint8_t ipd_stream[] = {
        '+', 'I', 'P', 'D', ',', '8', ':',
        0x41, 0x42, '\r', '\n', 0x00, 0x7F, 'O', 'K',
        '\r', '\n', 'C', 'L', 'O', 'S', 'E', 'D', '\r', '\n'
    };
    feed_chunk(&rx_rb, ipd_stream, sizeof(ipd_stream));
    at_poll(&at, 600u);

    /* 4. 命令超时验证 */
    at_send(&at, "AT+CIPSTART=\"TCP\",\"192.168.1.100\",8080",
            5000u, on_cmd_done, "CIPSTART");
    at_poll(&at, 5600u);

    return 0;
}
