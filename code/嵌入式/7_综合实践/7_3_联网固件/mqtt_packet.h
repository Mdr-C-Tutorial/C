#ifndef MQTT_PACKET_H
#define MQTT_PACKET_H

#include <stddef.h>
#include <stdint.h>

/*
 * MQTT 3.1.1 的最小报文构造器。
 *
 * 只覆盖本页用到的五种：CONNECT、SUBSCRIBE、PUBLISH（QoS 0）、PINGREQ、
 * DISCONNECT。固件里这些工作由 coreMQTT 完成，这里单独写一份是为了能
 * 把每一个字节摆出来讲清楚。
 */

/* 返回写出的字节数，写不下时返回 0 */
size_t mqtt_build_connect(uint8_t *out, size_t max, const char *client_id,
                          const char *will_topic, const char *will_payload,
                          uint16_t keepalive_s);
size_t mqtt_build_subscribe(uint8_t *out, size_t max, uint16_t packet_id,
                            const char *topic_filter);
size_t mqtt_build_publish(uint8_t *out, size_t max, const char *topic,
                          const char *payload);
size_t mqtt_build_pingreq(uint8_t *out, size_t max);
size_t mqtt_build_disconnect(uint8_t *out, size_t max);

#endif /* MQTT_PACKET_H */