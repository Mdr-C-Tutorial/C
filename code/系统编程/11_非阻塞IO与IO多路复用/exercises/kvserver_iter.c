#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_ENTRIES 4096
#define MAX_LINE 1024
#define RESP_CAP 512

struct entry {
    char key[64];
    char value[256];
    int used;
};

static struct entry entries[MAX_ENTRIES];

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static struct entry *find(const char *key) {
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (entries[i].used && strcmp(entries[i].key, key) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

/* 处理一行命令，把应答写进 resp，返回应答长度。 */
static size_t handle_line(const char *line, char *resp) {
    char key[64];
    char value[256];
    struct entry *e = NULL;

    if (sscanf(line, "SET %63s %255s", key, value) == 2) {
        e = find(key);
        if (e == NULL) {
            for (int i = 0; i < MAX_ENTRIES; i++) {
                if (!entries[i].used) {
                    e = &entries[i];
                    break;
                }
            }
        }
        if (e == NULL) {
            return (size_t)snprintf(resp, RESP_CAP, "ERROR table full\n");
        }
        snprintf(e->key, sizeof e->key, "%s", key);
        snprintf(e->value, sizeof e->value, "%s", value);
        e->used = 1;
        return (size_t)snprintf(resp, RESP_CAP, "OK\n");
    }
    if (sscanf(line, "GET %63s", key) == 1) {
        e = find(key);
        if (e == NULL) {
            return (size_t)snprintf(resp, RESP_CAP, "(nil)\n");
        }
        return (size_t)snprintf(resp, RESP_CAP, "VALUE %s\n", e->value);
    }
    if (sscanf(line, "DEL %63s", key) == 1) {
        e = find(key);
        if (e == NULL) {
            return (size_t)snprintf(resp, RESP_CAP, "0\n");
        }
        e->used = 0;
        return (size_t)snprintf(resp, RESP_CAP, "1\n");
    }
    return (size_t)snprintf(resp, RESP_CAP, "ERROR unknown command\n");
}

/* 迭代服务器：一次服务一个连接，读满一行处理一行。 */
static void serve(int fd) {
    char buf[MAX_LINE];
    size_t len = 0;
    for (;;) {
        ssize_t n = read(fd, buf + len, sizeof buf - len);
        if (n == 0) {
            return;
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        len += (size_t)n;
        size_t start = 0;
        for (;;) {
            char *nl = memchr(buf + start, '\n', len - start);
            if (nl == NULL) {
                break;
            }
            *nl = '\0';
            char resp[RESP_CAP];
            size_t rlen = handle_line(buf + start, resp);
            if (write(fd, resp, rlen) != (ssize_t)rlen) {
                return;
            }
            start = (size_t)(nl - buf) + 1;
        }
        memmove(buf, buf + start, len - start);
        len -= start;
        if (len == sizeof buf) {
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "21190";
    int listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listener == -1) {
        die("socket");
    }
    int one = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) ==
        -1) {
        die("setsockopt");
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)atoi(port)),
    };
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        die("inet_pton");
    }
    if (bind(listener, (struct sockaddr *)&addr, sizeof addr) == -1) {
        die("bind");
    }
    if (listen(listener, 16) == -1) {
        die("listen");
    }

    for (;;) {
        int fd = accept(listener, NULL, NULL);
        if (fd == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("accept");
        }
        serve(fd);
        close(fd);
    }
}