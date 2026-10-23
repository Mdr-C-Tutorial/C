#define _POSIX_C_SOURCE 200809L

#ifndef IMUD_MQTT_GLUE_H
#define IMUD_MQTT_GLUE_H

#include "imud.h"
#include "transport.h"

#include <stdbool.h>
#include <stdint.h>

#include "core_mqtt.h"

#define MQTT_BUFFER_SIZE 1024

/*
 * 一份 coreMQTT 上下文加三件事：TCP 传输、三条主题、命令主题收到的
 * rate 请求。主题在连接时按 topic_prefix/device_id 拼好。
 */
struct mqtt_client {
    struct NetworkContext net;
    MQTTContext_t mqtt;
    unsigned char buffer[MQTT_BUFFER_SIZE];
    MQTTFixedBuffer_t fixed;
    char topic_telemetry[128];
    char topic_status[128];
    char topic_cmd[128];
    volatile bool connected;
    volatile bool cmd_rate_pending;
    int cmd_rate_value;
};

void mqtt_client_init(struct mqtt_client *mc, const struct imud_config *cfg);

/* 建立 TCP 连接、完成 CONNECT 与 SUBSCRIBE。成功返回 0。 */
int mqtt_client_connect(struct mqtt_client *mc,
                        const struct imud_config *cfg);

void mqtt_client_disconnect(struct mqtt_client *mc, bool publish_offline);

/*
 * 调一次 coreMQTT 的 MQTT_ProcessLoop。返回 0 表示正常（包括没有数据），
 * -1 表示连接已经不可用，需要重连。
 */
int mqtt_client_poll(struct mqtt_client *mc);

int mqtt_client_publish_telemetry(struct mqtt_client *mc, int64_t ts,
                                  const struct imu_sample *s,
                                  const struct attitude *a, uint32_t seq);

#endif