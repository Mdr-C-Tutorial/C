/*
 * mqtt_packet.c：MQTT 3.1.1 报文构造。
 *
 * 固定头第一字节高 4 位是报文类型，低 4 位是标志；后面是剩余长度，
 * 用 7 位一组、最高位表示还有后续字节的可变长度编码。
 */
#include "mqtt_packet.h"

#include <string.h>

#define MQTT_TYPE_CONNECT 0x10u
#define MQTT_TYPE_CONNACK 0x20u
#define MQTT_TYPE_PUBLISH 0x30u
#define MQTT_TYPE_SUBSCRIBE 0x82u /* 类型 8 + 固定标志 0010 */
#define MQTT_TYPE_PINGREQ 0xC0u
#define MQTT_TYPE_DISCONNECT 0xE0u

/* 写剩余长度，返回占用的字节数（1～4） */
static size_t put_remaining_length(uint8_t *out, size_t len) {
    size_t n = 0;
    do {
        uint8_t byte = (uint8_t)(len % 128u);
        len /= 128u;
        if (len > 0u) {
            byte |= 0x80u;
        }
        out[n++] = byte;
    } while (len > 0u && n < 4u);
    return n;
}

static size_t put_string(uint8_t *out, size_t pos, const char *s) {
    size_t len = strlen(s);
    out[pos++] = (uint8_t)(len >> 8);
    out[pos++] = (uint8_t)(len & 0xFFu);
    memcpy(&out[pos], s, len);
    return pos + len;
}

size_t mqtt_build_connect(uint8_t *out, size_t max, const char *client_id,
                          const char *will_topic, const char *will_payload,
                          uint16_t keepalive_s) {
    uint8_t body[256];
    size_t p = 0;

    /* 可变头：协议名 "MQTT"、级别 4、连接标志、保活秒数 */
    p = put_string(body, p, "MQTT");
    body[p++] = 4u; /* 3.1.1 */

    uint8_t flags = 0x02u; /* 清会话 */
    if (will_topic != NULL) {
        flags |= 0x04u | 0x08u; /* 有遗嘱；bit3～4 为 01，即遗嘱 QoS 1 */
    }
    body[p++] = flags;
    body[p++] = (uint8_t)(keepalive_s >> 8);
    body[p++] = (uint8_t)(keepalive_s & 0xFFu);

    /* 负载：客户端标识，然后是遗嘱主题与遗嘱内容 */
    p = put_string(body, p, client_id);
    if (will_topic != NULL) {
        p = put_string(body, p, will_topic);
        p = put_string(body, p, will_payload != NULL ? will_payload : "");
    }

    size_t need = 1u + 4u + p;
    if (need > max) {
        return 0;
    }
    size_t q = 0;
    out[q++] = MQTT_TYPE_CONNECT;
    q += put_remaining_length(&out[q], p);
    memcpy(&out[q], body, p);
    return q + p;
}

size_t mqtt_build_subscribe(uint8_t *out, size_t max, uint16_t packet_id,
                            const char *topic_filter) {
    uint8_t body[128];
    size_t p = 0;

    body[p++] = (uint8_t)(packet_id >> 8);
    body[p++] = (uint8_t)(packet_id & 0xFFu);
    p = put_string(body, p, topic_filter);
    body[p++] = 0x00u; /* 请求的 QoS 0 */

    size_t need = 1u + 4u + p;
    if (need > max) {
        return 0;
    }
    size_t q = 0;
    out[q++] = MQTT_TYPE_SUBSCRIBE;
    q += put_remaining_length(&out[q], p);
    memcpy(&out[q], body, p);
    return q + p;
}

size_t mqtt_build_publish(uint8_t *out, size_t max, const char *topic,
                          const char *payload) {
    size_t topic_len = strlen(topic);
    size_t payload_len = strlen(payload);
    size_t p = 2u + topic_len + payload_len;

    size_t need = 1u + 4u + p;
    if (need > max) {
        return 0;
    }
    size_t q = 0;
    out[q++] = MQTT_TYPE_PUBLISH; /* QoS 0，无报文标识符 */
    q += put_remaining_length(&out[q], p);
    q = put_string(out, q, topic);
    memcpy(&out[q], payload, payload_len);
    return q + payload_len;
}

size_t mqtt_build_pingreq(uint8_t *out, size_t max) {
    if (max < 2u) {
        return 0;
    }
    out[0] = MQTT_TYPE_PINGREQ;
    out[1] = 0x00u;
    return 2u;
}

size_t mqtt_build_disconnect(uint8_t *out, size_t max) {
    if (max < 2u) {
        return 0;
    }
    out[0] = MQTT_TYPE_DISCONNECT;
    out[1] = 0x00u;
    return 2u;
}