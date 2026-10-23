/*
 * resp_client.c —— 用 RESP 字节直接和 Redis 对话的最小客户端。
 *
 * 用法：
 *   resp_client <host> <port> <命令> [参数...] [-- <命令> [参数...] ...]
 *
 * 一条命令编码成一个多批量请求；用 "--" 分隔的多条命令会先拼进同一块
 * 缓冲区，再用一次 write 发送，这就是流水线。程序把请求字节和回复字节
 * 都按 C 转义序列打印出来，方便和 od -c 的结果对照。
 *
 * 只用于学习协议，不解析回复的语义。
 */
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct buffer {
    char *data;
    size_t len;
    size_t cap;
};

static void buffer_init(struct buffer *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buffer_free(struct buffer *b) {
    free(b->data);
    buffer_init(b);
}

static int buffer_reserve(struct buffer *b, size_t extra) {
    if (b->len + extra <= b->cap) {
        return 0;
    }
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + extra) {
        cap *= 2;
    }
    char *p = realloc(b->data, cap);
    if (p == NULL) {
        return -1;
    }
    b->data = p;
    b->cap = cap;
    return 0;
}

static int buffer_append(struct buffer *b, const void *data, size_t len) {
    if (buffer_reserve(b, len) != 0) {
        return -1;
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
    return 0;
}

static int buffer_append_str(struct buffer *b, const char *s) {
    return buffer_append(b, s, strlen(s));
}

/* 把一条命令编码成 RESP 的多批量请求：*<参数个数>\r\n，然后每个参数
 * 写成 $<字节数>\r\n<内容>\r\n。 */
static int encode_command(struct buffer *b, int argc, char **argv) {
    char head[32];
    int n = snprintf(head, sizeof head, "*%d\r\n", argc);
    if (buffer_append(b, head, (size_t)n) != 0) {
        return -1;
    }
    for (int i = 0; i < argc; i++) {
        n = snprintf(head, sizeof head, "$%zu\r\n", strlen(argv[i]));
        if (buffer_append(b, head, (size_t)n) != 0) {
            return -1;
        }
        if (buffer_append_str(b, argv[i]) != 0 ||
            buffer_append_str(b, "\r\n") != 0) {
            return -1;
        }
    }
    return 0;
}

static int write_all(int fd, const char *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, data + off, len - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int read_all(int fd, struct buffer *b) {
    char chunk[4096];
    for (;;) {
        ssize_t n = read(fd, chunk, sizeof chunk);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        if (buffer_append(b, chunk, (size_t)n) != 0) {
            return -1;
        }
    }
}

/* 按 C 转义序列打印字节，不可打印的字节写成 \xHH。 */
static void print_escaped(const struct buffer *b) {
    for (size_t i = 0; i < b->len; i++) {
        unsigned char c = (unsigned char)b->data[i];
        if (c == '\r') {
            fputs("\\r", stdout);
        } else if (c == '\n') {
            fputs("\\n", stdout);
        } else if (c == '\\') {
            fputs("\\\\", stdout);
        } else if (c >= 32 && c < 127) {
            putchar(c);
        } else {
            printf("\\x%02x", c);
        }
    }
    putchar('\n');
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "用法：%s <host> <port> <命令> [参数...] "
                        "[-- <命令> [参数...] ...]\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    char *end = NULL;
    long port = strtol(argv[2], &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "端口不合法：%s\n", argv[2]);
        return 1;
    }

    struct buffer request;
    buffer_init(&request);
    int start = 3;
    int ok = 0;
    for (int i = 3; i <= argc; i++) {
        if (i == argc || strcmp(argv[i], "--") == 0) {
            if (i == start) {
                fprintf(stderr, "空命令\n");
                ok = -1;
                break;
            }
            if (encode_command(&request, i - start, argv + start) != 0) {
                ok = -1;
                break;
            }
            start = i + 1;
        }
    }
    if (ok != 0) {
        buffer_free(&request);
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        buffer_free(&request);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "地址不合法：%s\n", host);
        close(fd);
        buffer_free(&request);
        return 1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect");
        close(fd);
        buffer_free(&request);
        return 1;
    }

    if (write_all(fd, request.data, request.len) != 0) {
        perror("write");
        close(fd);
        buffer_free(&request);
        return 1;
    }
    /* 告诉服务器请求到此为止，服务器发完回复后会关闭连接。 */
    shutdown(fd, SHUT_WR);

    struct buffer reply;
    buffer_init(&reply);
    if (read_all(fd, &reply) != 0) {
        perror("read");
        close(fd);
        buffer_free(&request);
        buffer_free(&reply);
        return 1;
    }

    printf("请求 %zu 字节：\n", request.len);
    print_escaped(&request);
    printf("回复 %zu 字节：\n", reply.len);
    print_escaped(&reply);

    close(fd);
    buffer_free(&request);
    buffer_free(&reply);
    return 0;
}