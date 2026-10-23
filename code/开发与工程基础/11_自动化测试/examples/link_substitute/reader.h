/*
 * 读文件模块的接口。正式实现是 reader.c，测试里换成 fake_reader.c。
 */
#ifndef READER_H
#define READER_H

#include <stddef.h>

/* 读入 path 的全部内容；成功时返回缓冲区（由调用方 free），字节数写入 *len */
char *reader_load(const char *path, size_t *len);

#endif
