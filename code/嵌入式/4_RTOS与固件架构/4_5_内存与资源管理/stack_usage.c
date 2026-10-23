/*
 * stack_usage.c：配合 arm-none-eabi-gcc -fstack-usage 观察各函数栈帧大小。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    int32_t x_est;
    int32_t p_cov;
    int32_t q_noise;
    int32_t r_noise;
} kalman1d_t;

extern int16_t hw_read_reg16(uint8_t reg);
extern void uart_write_bytes(const char *buf, size_t len);
extern void task_delay_ms(uint32_t ms);

__attribute__((noinline))
int32_t sensor_read_filtered(kalman1d_t *kf) {
    int32_t raw_window[8];
    int32_t sum = 0;
    for (int i = 0; i < 8; ++i) {
        raw_window[i] = hw_read_reg16((uint8_t)i);
        sum += raw_window[i];
    }
    int32_t meas = sum / 8;
    int32_t p_pred = kf->p_cov + kf->q_noise;
    int32_t gain_num = p_pred;
    int32_t gain_den = p_pred + kf->r_noise;
    kf->x_est = kf->x_est + (gain_num * (meas - kf->x_est)) / gain_den;
    kf->p_cov = ((gain_den - gain_num) * p_pred) / gain_den;
    return kf->x_est;
}

/* 任务 A：纯整数控制任务，自身栈帧与被调函数栈帧都很小 */
void control_task(void *arg) {
    (void)arg;
    kalman1d_t kf = {0, 100, 4, 25};
    for (;;) {
        int32_t val = sensor_read_filtered(&kf);
        if (val > 1000) {
            uart_write_bytes("HIGH\r\n", 6);
        }
        task_delay_ms(10);
    }
}

/* 任务 B：在栈上开 128 字节缓冲区并调用 snprintf 格式化报文 */
void telemetry_task(void *arg) {
    (void)arg;
    kalman1d_t kf = {0, 100, 4, 25};
    uint32_t seq = 0;
    for (;;) {
        int32_t val = sensor_read_filtered(&kf);
        char line[128];
        int n = snprintf(line, sizeof line,
                         "{\"seq\":%lu,\"az\":%ld}\r\n",
                         (unsigned long)seq++, (long)val);
        if (n > 0) {
            uart_write_bytes(line, (size_t)n);
        }
        task_delay_ms(100);
    }
}
