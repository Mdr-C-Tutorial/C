/*
 * sim_support.h：4.3 节示例共用的模拟传感器数据。
 *
 * 控制台输出与示例收尾由 4.2 节建立的 common/freertos/rtos_console.h 提供，
 * 这里只补充第 4 章统一的 sample_t 与查表实现的假传感器。
 */
#ifndef SIM_SUPPORT_H
#define SIM_SUPPORT_H

#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

typedef struct {
    TickType_t tick;
    int16_t ax;
    int16_t ay;
    int16_t az;
} sample_t;

/* 按序号从固定的表里取一组加速度值，模拟 MPU6050 的读数 */
static inline void fake_sensor_read(size_t index, sample_t *out) {
    static const int16_t table[][3] = {
        { 12, -18,  998 },
        { 16, -14, 1006 },
        { 10, -20, 1002 },
        { 14, -16,  994 },
        { 18, -12, 1010 },
        {  8, -22,  990 },
        { 15, -15, 1004 },
        { 11, -19,  996 },
    };
    size_t n = sizeof(table) / sizeof(table[0]);
    size_t i = index % n;
    out->tick = xTaskGetTickCount();
    out->ax = table[i][0];
    out->ay = table[i][1];
    out->az = table[i][2];
}

#endif /* SIM_SUPPORT_H */
