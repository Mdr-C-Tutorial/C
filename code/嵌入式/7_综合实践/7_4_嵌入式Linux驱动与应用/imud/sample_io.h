#define _POSIX_C_SOURCE 200809L

#ifndef IMUD_SAMPLE_IO_H
#define IMUD_SAMPLE_IO_H

#include "imud.h"

#include <stddef.h>

#define SAMPLE_READER_CAPACITY 4096

/*
 * 字节流到样本的拼装器。一次 read 拿到的字节数不一定是 24 的整数倍，
 * 半个样本留到下一次。驱动、命名管道、文件都按同样的字节流对待。
 */
struct sample_reader {
    unsigned char buf[SAMPLE_READER_CAPACITY];
    size_t len;
};

void sample_reader_init(struct sample_reader *sr);

/*
 * 把 data 里 n 个字节追加进去，再尽可能多地取出完整样本。
 * 返回写入 out[0..cap) 的样本数；单个样本超过 out 剩余空间时停止，
 * 剩下的字节留在内部缓冲里等下次。
 */
size_t sample_reader_feed(struct sample_reader *sr, const unsigned char *data,
                          size_t n, struct imu_sample *out, size_t cap);

/* 内部缓冲里还剩多少字节（不是整样本的部分） */
size_t sample_reader_pending(const struct sample_reader *sr);

/* 24 字节的线上表示：小端，字段顺序与 struct imu_sample 一致 */
void sample_to_le_bytes(const struct imu_sample *s, unsigned char out[24]);
struct imu_sample sample_from_le_bytes(const unsigned char in[24]);

#endif