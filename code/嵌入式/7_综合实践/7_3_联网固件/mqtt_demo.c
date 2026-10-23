/*
 * mqtt_demo.c：在 127.0.0.1 上跑一个最小 MQTT 3.1.1 回路。
 *
 * 一个进程里同时放两个角色：
 *   - 伪服务器：监听 1883，处理 CONNECT、SUBSCRIBE、PUBLISH、PINGREQ；
 *   - 设备端：用与固件相同的打包函数发 CONNECT、SUBSCRIBE、PUBLISH，
 *     并订阅自己的命令主题。
 * 两边都只实现本页要讲的那几个报文，够用来核对字节与联调流程。
 *
 * 编译：gcc -std=c17 -Wall -Wextra mqtt_demo.c -lws2_32 -o mqtt_demo
 */
#define _WIN32_WINNT 0x0601

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "mqtt_packet.h"

#define PORT 25731
#define BUF_SIZE 1024

struct broker_result {
    uint16_t msg_id;
    char topic[64];
    char payload[128];
};

static SOCKET g_listener;

static int run_broker(SOCKET listener, struct broker_result *result);

static void hexdump(const char *tag, const uint8_t *p, size_t n) {
    printf("%s（%u 字节）：", tag, (unsigned)n);
    for (size_t i = 0; i < n; ++i) {
        printf("%02X ", p[i]);
    }
    printf("\n");
}

/* 从套接字读一个完整的 MQTT 报文（固定头 + 剩余长度 + 余下部分） */
static int recv_packet(SOCKET s, uint8_t *buf, size_t max) {
    int n = recv(s, (char *)buf, 1, 0);
    if (n != 1) {
        return -1;
    }
    size_t len = 0;
    size_t mult = 1;
    for (;;) {
        uint8_t b;
        if (recv(s, (char *)&b, 1, 0) != 1) {
            return -1;
        }
        buf[1 + len++] = b;
        if (1 + len >= max) {
            return -1;
        }
        if ((b & 0x80u) == 0u) {
            break;
        }
        mult *= 128u;
        if (mult > 128u * 128u * 128u) {
            return -1;
        }
    }
    size_t remaining = 0;
    size_t m = 1;
    for (size_t i = 1; i <= len; ++i) {
        remaining += (size_t)(buf[i] & 0x7Fu) * m;
        m *= 128u;
    }
    size_t got = 1u + len;
    while (got < 1u + len + remaining) {
        int r = recv(s, (char *)buf + got, (int)(1u + len + remaining - got), 0);
        if (r <= 0) {
            return -1;
        }
        got += (size_t)r;
    }
    return (int)(1u + len + remaining);
}

static SOCKET start_listener(void) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(s, (struct sockaddr *)&addr, sizeof addr) != 0) {
        printf("bind 失败：%d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }
    listen(s, 1);
    return s;
}

/* 伪服务器：返回 0 表示正常 */
static int run_broker(SOCKET listener, struct broker_result *result) {
    SOCKET c = accept(listener, NULL, NULL);
    if (c == INVALID_SOCKET) {
        return -1;
    }

    uint8_t pkt[BUF_SIZE];
    int n = recv_packet(c, pkt, sizeof pkt);
    if (n < 0) {
        return -1;
    }
    printf("[服务器] 收到 CONNECT：");
    hexdump("", pkt, (size_t)n);

    /* CONNACK：会话不存在、返回码 0 */
    const uint8_t connack[4] = {0x20, 0x02, 0x00, 0x00};
    send(c, (const char *)connack, sizeof connack, 0);

    n = recv_packet(c, pkt, sizeof pkt);
    if (n < 0) {
        return -1;
    }
    printf("[服务器] 收到 SUBSCRIBE：");
    hexdump("", pkt, (size_t)n);
    /* SUBACK：报文标识符取自 SUBSCRIBE 的第 2、3 字节之后 */
    uint16_t id = (uint16_t)((pkt[2] << 8) | pkt[3]);
    const uint8_t suback[5] = {0x90, 0x03, (uint8_t)(id >> 8), (uint8_t)id, 0x00};
    send(c, (const char *)suback, sizeof suback, 0);

    /* 之后依次收 PUBLISH 与 PINGREQ */
    for (int i = 0; i < 3; ++i) {
        n = recv_packet(c, pkt, sizeof pkt);
        if (n < 0) {
            break;
        }
        uint8_t type = pkt[0] >> 4;
        if (type == 3) {
            /* 主题长度两字节，之后是主题、报文标识符（QoS 0 时没有）、负载 */
            size_t topic_len = (size_t)((pkt[2] << 8) | pkt[3]);
            size_t off = 4u + topic_len;
            size_t payload_len = (size_t)n - off;
            snprintf(result->topic, sizeof result->topic, "%.*s", (int)topic_len,
                     (const char *)&pkt[4]);
            snprintf(result->payload, sizeof result->payload, "%.*s", (int)payload_len,
                     (const char *)&pkt[off]);
            result->msg_id = (uint16_t)(result->msg_id + 1u);
            printf("[服务器] 收到 PUBLISH：主题 %s，负载 %s\n", result->topic,
                   result->payload);
        } else if (type == 12) {
            const uint8_t pingresp[2] = {0xD0, 0x00};
            send(c, (const char *)pingresp, sizeof pingresp, 0);
            printf("[服务器] 收到 PINGREQ，已回 PINGRESP\n");
        } else if (type == 14) {
            printf("[服务器] 收到 DISCONNECT\n");
            break;
        }
    }
    closesocket(c);
    return 0;
}

static DWORD WINAPI broker_thread(LPVOID arg) {
    struct broker_result *result = (struct broker_result *)arg;
    (void)run_broker(g_listener, result);
    return 0;
}

int main(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup 失败\n");
        return 1;
    }

    g_listener = start_listener();
    if (g_listener == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    /* 伪服务器放进单独的线程：设备侧发报文与服务器收报文按各自节奏进行 */
    struct broker_result result;
    memset(&result, 0, sizeof result);
    HANDLE broker = CreateThread(NULL, 0, broker_thread, &result, 0, NULL);

    /* 1. 设备侧先建报文，再连接 */
    uint8_t out[BUF_SIZE];
    const char *client_id = "mdr-0001";
    const char *will_topic = "mdr/0001/status";
    size_t n = mqtt_build_connect(out, sizeof out, client_id, will_topic, "offline", 30);
    printf("[设备] CONNECT 报文 %u 字节\n", (unsigned)n);
    hexdump("       ", out, n);

    SOCKET dev = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(dev, (struct sockaddr *)&addr, sizeof addr) != 0) {
        printf("连接伪服务器失败：%d\n", WSAGetLastError());
        return 1;
    }
    send(dev, (const char *)out, (int)n, 0);

    uint8_t in[BUF_SIZE];
    int rn = recv_packet(dev, in, sizeof in);
    printf("[设备] 收到 CONNACK：");
    hexdump("", in, (size_t)rn);

    /* 2. 订阅命令主题 */
    n = mqtt_build_subscribe(out, sizeof out, 2, "mdr/0001/cmd");
    send(dev, (const char *)out, (int)n, 0);
    rn = recv_packet(dev, in, sizeof in);
    printf("[设备] 收到 SUBACK：");
    hexdump("", in, (size_t)rn);

    /* 3. 发布遥测 */
    const char *json =
        "{\"ts\":1761200000,\"ax\":12,\"ay\":-340,\"az\":995,\"pitch\":-1.9,\"roll\":19.0,"
        "\"seq\":1}";
    n = mqtt_build_publish(out, sizeof out, "mdr/0001/telemetry", json);
    send(dev, (const char *)out, (int)n, 0);
    printf("[设备] PUBLISH 报文 %u 字节\n", (unsigned)n);

    /* 4. 保活 */
    n = mqtt_build_pingreq(out, sizeof out);
    send(dev, (const char *)out, (int)n, 0);
    rn = recv_packet(dev, in, sizeof in);
    printf("[设备] 收到 PINGRESP：");
    hexdump("", in, (size_t)rn);

    /* 5. 正常断开 */
    n = mqtt_build_disconnect(out, sizeof out);
    send(dev, (const char *)out, (int)n, 0);

    WaitForSingleObject(broker, 5000);
    CloseHandle(broker);
    printf("[结果] 收到 %u 条 PUBLISH，最后一条：%s -> %s\n", (unsigned)result.msg_id,
           result.topic, result.payload);

    closesocket(dev);
    closesocket(g_listener);
    WSACleanup();
    return result.msg_id == 1u ? 0 : 1;
}