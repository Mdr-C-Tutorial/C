#define _POSIX_C_SOURCE 200809L

#include "mqtt_glue.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core_mqtt.h"
#include "transport_interface.h"

#define MQTT_KEEPALIVE_SECONDS 30
#define MQTT_CONNECT_TIMEOUT_MS 5000

/* coreMQTT v2.3.1 的回调里拿不到用户数据，单实例服务用一个文件级指针 */
static struct mqtt_client *g_mc;

static uint32_t now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u +
                      (uint64_t)(ts.tv_nsec / 1000000));
}

static void on_mqtt_event(MQTTContext_t *ctx, MQTTPacketInfo_t *packet,
                          MQTTDeserializedInfo_t *info) {
    struct mqtt_client *mc = g_mc;

    (void)ctx;
    (void)packet;
    if (info->pPublishInfo == NULL) {
        return;
    }

    const MQTTPublishInfo_t *pub = info->pPublishInfo;

    if ((size_t)pub->topicNameLength == strlen(mc->topic_cmd) &&
        strncmp(pub->pTopicName, mc->topic_cmd, pub->topicNameLength) == 0) {
        /* 命令主题只认 rate=<数字>，其余留给 OTA 与 reboot 用 */
        char payload[64];
        size_t n = pub->payloadLength < sizeof payload - 1
                       ? pub->payloadLength
                       : sizeof payload - 1;

        memcpy(payload, pub->pPayload, n);
        payload[n] = '\0';

        int rate = 0;

        if (sscanf(payload, "rate=%d", &rate) == 1 && rate >= 4 &&
            rate <= 1000) {
            mc->cmd_rate_value = rate;
            mc->cmd_rate_pending = true;
        }
    }
}

static TransportInterface_t make_transport(struct mqtt_client *mc) {
    TransportInterface_t t = { 0 };

    t.recv = (TransportRecv_t)transport_recv;
    t.send = (TransportSend_t)transport_send;
    t.pNetworkContext = &mc->net;
    return t;
}

void mqtt_client_init(struct mqtt_client *mc, const struct imud_config *cfg) {
    memset(mc, 0, sizeof *mc);
    mc->net.fd = -1;
    mc->net.recv_timeout_ms = 100;
    snprintf(mc->topic_telemetry, sizeof mc->topic_telemetry, "%s/%s/telemetry",
             cfg->topic_prefix, cfg->device_id);
    snprintf(mc->topic_status, sizeof mc->topic_status, "%s/%s/status",
             cfg->topic_prefix, cfg->device_id);
    snprintf(mc->topic_cmd, sizeof mc->topic_cmd, "%s/%s/cmd",
             cfg->topic_prefix, cfg->device_id);
}

int mqtt_client_connect(struct mqtt_client *mc,
                        const struct imud_config *cfg) {
    TransportInterface_t transport;
    MQTTConnectInfo_t connect_info = { 0 };
    MQTTPublishInfo_t will_info = { 0 };
    MQTTStatus_t status;
    bool session_present = false;
    MQTTSubscribeInfo_t sub = { 0 };
    const char *client_id = cfg->device_id;

    if (transport_connect(&mc->net, cfg->host, cfg->port) < 0) {
        return -1;
    }
    transport = make_transport(mc);
    mc->fixed.pBuffer = mc->buffer;
    mc->fixed.size = sizeof mc->buffer;
    g_mc = mc;
    status = MQTT_Init(&mc->mqtt, &transport, now_ms, on_mqtt_event,
                       &mc->fixed);
    if (status != MQTTSuccess) {
        fprintf(stderr, "MQTT_Init: %s\n", MQTT_Status_strerror(status));
        transport_close(&mc->net);
        return -1;
    }

    connect_info.cleanSession = true;
    connect_info.keepAliveSeconds = MQTT_KEEPALIVE_SECONDS;
    connect_info.pClientIdentifier = client_id;
    connect_info.clientIdentifierLength = (uint16_t)strlen(client_id);

    /* 遗嘱：进程退出或连接断掉后，服务器代发 offline */
    /*
     * 遗嘱与状态用 QoS 0：这个服务没有维护 QoS 状态机（MQTT_InitStatefulQoS），
     * 而离线通知丢一条没有关系，keepalive 超时后服务器会再判一次。
     * 7.3 的固件用 QoS 1，那是因为它已经初始化了状态机。
     */
    will_info.qos = MQTTQoS0;
    will_info.pTopicName = mc->topic_status;
    will_info.topicNameLength = (uint16_t)strlen(mc->topic_status);
    will_info.pPayload = "offline";
    will_info.payloadLength = 7;

    status = MQTT_Connect(&mc->mqtt, &connect_info, &will_info,
                          MQTT_CONNECT_TIMEOUT_MS, &session_present);
    if (status != MQTTSuccess) {
        fprintf(stderr, "MQTT_Connect: %s\n", MQTT_Status_strerror(status));
        transport_close(&mc->net);
        return -1;
    }

    sub.qos = MQTTQoS0;
    sub.pTopicFilter = mc->topic_cmd;
    sub.topicFilterLength = (uint16_t)strlen(mc->topic_cmd);
    status = MQTT_Subscribe(&mc->mqtt, &sub, 1, MQTT_GetPacketId(&mc->mqtt));
    if (status != MQTTSuccess) {
        fprintf(stderr, "MQTT_Subscribe: %s\n", MQTT_Status_strerror(status));
        transport_close(&mc->net);
        return -1;
    }

    /*
     * SUBSCRIBE 的 SUBACK 要靠 ProcessLoop 收。这里转两圈把它取走后
     * 再交给事件循环的轮询定时器，避免第一条 PUBLISH 抢在 SUBACK 前面。
     */
    for (int i = 0; i < 2; i++) {
        status = MQTT_ProcessLoop(&mc->mqtt);
        if (status != MQTTSuccess && status != MQTTNoDataAvailable &&
            status != MQTTNeedMoreBytes) {
            fprintf(stderr, "MQTT_ProcessLoop: %s\n",
                    MQTT_Status_strerror(status));
            transport_close(&mc->net);
            return -1;
        }
    }
    mc->connected = true;
    printf("已连接 %s:%s，主题 %s\n", cfg->host, cfg->port, mc->topic_telemetry);
    return 0;
}

void mqtt_client_disconnect(struct mqtt_client *mc, bool publish_offline) {
    if (mc->net.fd < 0) {
        return;
    }
    if (mc->connected) {
        if (publish_offline) {
            MQTTPublishInfo_t pub = { 0 };

            pub.qos = MQTTQoS0;
            pub.pTopicName = mc->topic_status;
            pub.topicNameLength = (uint16_t)strlen(mc->topic_status);
            pub.pPayload = "offline";
            pub.payloadLength = 7;
            MQTT_Publish(&mc->mqtt, &pub, 0);
        }
        MQTT_Disconnect(&mc->mqtt);
    }
    mc->connected = false;
    transport_close(&mc->net);
}

int mqtt_client_poll(struct mqtt_client *mc) {
    MQTTStatus_t status;

    if (!mc->connected) {
        return -1;
    }
    status = MQTT_ProcessLoop(&mc->mqtt);
    switch (status) {
    case MQTTSuccess:
    case MQTTNoDataAvailable:
    case MQTTNeedMoreBytes: /* 报文还没收全，下一轮继续 */
        return 0;
    default:
        fprintf(stderr, "MQTT_ProcessLoop: %s\n",
                MQTT_Status_strerror(status));
        mc->connected = false;
        return -1;
    }
}

int mqtt_client_publish_telemetry(struct mqtt_client *mc, int64_t ts,
                                  const struct imu_sample *s,
                                  const struct attitude *a, uint32_t seq) {
    char payload[192];
    MQTTPublishInfo_t pub = { 0 };
    MQTTStatus_t status;
    int n;

    /*
     * 负载与 7.3 完全一致：ts 是 Unix 秒，ax/ay/az 是 mg，
     * pitch/roll 乘 100 取整成 0.01°，seq 是本进程发出的序号。
     */
    n = snprintf(payload, sizeof payload,
                 "{\"ts\":%lld,\"ax\":%d,\"ay\":%d,\"az\":%d,"
                 "\"pitch\":%ld,\"roll\":%ld,\"seq\":%lu}",
                 (long long)ts, s->accel[0], s->accel[1], s->accel[2],
                 lround(a->pitch * 100.0), lround(a->roll * 100.0),
                 (unsigned long)seq);
    if (n < 0 || (size_t)n >= sizeof payload) {
        return -1;
    }

    pub.qos = MQTTQoS0;
    pub.pTopicName = mc->topic_telemetry;
    pub.topicNameLength = (uint16_t)strlen(mc->topic_telemetry);
    pub.pPayload = payload;
    pub.payloadLength = (size_t)n;

    status = MQTT_Publish(&mc->mqtt, &pub, 0);
    if (status != MQTTSuccess) {
        fprintf(stderr, "MQTT_Publish: %s\n", MQTT_Status_strerror(status));
        mc->connected = false;
        return -1;
    }
    return 0;
}