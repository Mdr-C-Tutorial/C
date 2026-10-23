#ifndef CORE_MQTT_CONFIG_H
#define CORE_MQTT_CONFIG_H

/*
 * coreMQTT 的编译配置。注意这些宏在库自己的 .c 文件里被使用，
 * 只在应用侧定义不起作用；所以 Buildroot 的 coremqtt 包与 imud
 * 必须用同一份头文件（见第 4 节）。
 */

/* 日志：产品映像里关掉，省代码；调试时改成 1 并重编 */
#ifndef IMUD_MQTT_LOG
#define IMUD_MQTT_LOG 0
#endif

#if IMUD_MQTT_LOG
#include <stdio.h>
#define LogError(message) printf message
#define LogWarn(message) printf message
#define LogInfo(message) printf message
#define LogDebug(message) printf message
#else
#define LogError(message)
#define LogWarn(message)
#define LogInfo(message)
#define LogDebug(message)
#endif

/*
 * 已经检测到报文、等剩余字节时的最长轮询时间。传输层的 recv 最多阻塞
 * TRANSPORT_RECV_TIMEOUT_MS（10 ms）就返回 0，所以这项要比它大，
 * 否则库会先于传输层放弃。取 30 ms。
 */
#define MQTT_RECV_POLLING_TIMEOUT_MS 30U

/*
 * SUBSCRIBE 的 writev 向量数：每个主题固定占 3 个（长度、主题、QoS），
 * 所以至少要是 3；本服务只订一个主题，写 4 留一点余量。
 */
#define MQTT_SUB_UNSUB_MAX_VECTORS 4U

#endif