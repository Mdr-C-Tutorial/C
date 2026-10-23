#define _POSIX_C_SOURCE 200809L

#include "server.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_usage(FILE *out, const char *program) {
    fprintf(out,
            "Usage: %s [options]\n"
            "  -b ADDR   IPv4 address to listen on (default 127.0.0.1)\n"
            "  -p PORT   port to listen on (default 8080)\n"
            "  -r DIR    document root (default www)\n"
            "  -c N      maximum concurrent connections (default 1024)\n"
            "  -m BYTES  maximum size of a request head (default 8192)\n"
            "  -t MS     idle timeout in milliseconds (default 10000)\n"
            "  -q        log errors only\n"
            "  -v        also log connection events\n"
            "  -h        show this help\n",
            program);
}

// 解析整数参数；不合法时说明原因并以状态 2 退出（命令行用法错误的惯例）。
static long parse_number(int option, const char *text, long min, long max) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < min ||
        value > max) {
        fprintf(stderr, "-%c: expected an integer in [%ld, %ld], got \"%s\"\n",
                option, min, max, text);
        exit(2);
    }
    return value;
}

int main(int argc, char *argv[]) {
    struct server_config config = {
        .address = "127.0.0.1", // 默认只接受本机的连接
        .port = 8080,
        .root = "www",
        .max_connections = 1024,
        .max_request_size = 8192,
        .idle_timeout_ms = 10000,
        .log_level = LOG_INFO,
    };

    int option;
    while ((option = getopt(argc, argv, "b:p:r:c:m:t:qvh")) != -1) {
        switch (option) {
        case 'b':
            config.address = optarg;
            break;
        case 'p':
            config.port = (int)parse_number(option, optarg, 1, 65535);
            break;
        case 'r':
            config.root = optarg;
            break;
        case 'c':
            config.max_connections =
                (int)parse_number(option, optarg, 1, 100000);
            break;
        case 'm':
            config.max_request_size =
                (size_t)parse_number(option, optarg, 256, 1048576);
            break;
        case 't':
            config.idle_timeout_ms =
                (int)parse_number(option, optarg, 100, 3600000);
            break;
        case 'q':
            config.log_level = LOG_ERROR;
            break;
        case 'v':
            config.log_level = LOG_DEBUG;
            break;
        case 'h':
            print_usage(stdout, argv[0]);
            return 0;
        default:
            print_usage(stderr, argv[0]);
            return 2;
        }
    }
    if (optind < argc) {
        print_usage(stderr, argv[0]);
        return 2;
    }

    struct server srv;
    bool ok = server_init(&srv, &config) == 0 && server_run(&srv) == 0;
    server_cleanup(&srv);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
