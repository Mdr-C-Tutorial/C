#define _POSIX_C_SOURCE 200809L

#include "net.h"
#include "protocol.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* 最长的一行输入是 "/msg 昵称 正文"，在正文上限之外留出命令和空格的余量 */
#define INPUT_MAX (PROTO_TEXT_MAX + PROTO_NICK_MAX + 16)

typedef struct {
    int fd;
    frame_buffer in;      /* 来自服务器的字节，按帧切分 */
    char line[INPUT_MAX]; /* 来自标准输入的字节，按行切分 */
    size_t line_len;
    bool skipping; /* 正在丢弃一行过长的输入，直到下一个换行 */
} chat_client;

typedef enum {
    INPUT_OK,   /* 继续运行 */
    INPUT_QUIT, /* 用户输入了 /quit */
    INPUT_EOF   /* 标准输入结束 */
} input_result;

static void print_help(void) {
    puts("commands:\n"
         "  /nick NAME       change your nickname\n"
         "  /msg NAME TEXT   send a private message\n"
         "  /list            show who is online\n"
         "  /quit            leave the chat\n"
         "any other line is sent to everyone");
}

/* 发送失败只打印原因：连接若真的断了，poll 会报告，由 read_server 结束程序 */
static void send_message(int fd, msg_type type, const char *name,
                         const char *text) {
    message msg;
    unsigned char frame[PROTO_FRAME_MAX];
    message_set(&msg, type, name, text);
    size_t len = proto_encode(&msg, frame, sizeof frame);
    if (net_send_all(fd, frame, len) < 0) {
        perror("send");
    }
}

static void print_message(const message *msg) {
    switch (msg->type) {
    case MSG_WELCOME:
    case MSG_NOTICE:
        printf("* %s\n", msg->text);
        break;
    case MSG_CHAT:
        printf("<%s> %s\n", msg->name, msg->text);
        break;
    case MSG_PRIVATE:
        printf("[private] <%s> %s\n", msg->name, msg->text);
        break;
    case MSG_USERS:
        printf("* online: %s\n", msg->text);
        break;
    case MSG_ERROR:
        printf("! %s\n", msg->text);
        break;
    default:
        printf("? unexpected %s message\n", msg_type_name(msg->type));
        break;
    }
}

/* 在 s 的第一个空格处截断，返回其后的部分（跳过连续的空格）。 */
static char *split_word(char *s) {
    char *space = strchr(s, ' ');
    if (space == NULL) {
        return s + strlen(s);
    }
    *space = '\0';
    char *rest = space + 1;
    while (*rest == ' ') {
        rest++;
    }
    return rest;
}

static input_result run_command(int fd, char *line) {
    char *cmd = line + 1; /* 跳过 '/' */
    char *args = split_word(cmd);

    if (strcmp(cmd, "nick") == 0) {
        if (!nick_is_valid(args)) {
            fprintf(stderr, "usage: /nick NAME "
                            "(1-16 letters, digits, '_' or '-')\n");
            return INPUT_OK;
        }
        send_message(fd, MSG_NICK, args, NULL);
    } else if (strcmp(cmd, "msg") == 0) {
        char *text = split_word(args);
        if (!nick_is_valid(args) || text[0] == '\0') {
            fprintf(stderr, "usage: /msg NAME TEXT\n");
            return INPUT_OK;
        }
        if (strlen(text) > PROTO_TEXT_MAX) {
            fprintf(stderr, "message too long (max %d bytes)\n",
                    PROTO_TEXT_MAX);
            return INPUT_OK;
        }
        send_message(fd, MSG_WHISPER, args, text);
    } else if (strcmp(cmd, "list") == 0) {
        send_message(fd, MSG_LIST, NULL, NULL);
    } else if (strcmp(cmd, "quit") == 0) {
        return INPUT_QUIT;
    } else if (strcmp(cmd, "help") == 0) {
        print_help();
    } else {
        fprintf(stderr, "unknown command: /%s (try /help)\n", cmd);
    }
    return INPUT_OK;
}

static input_result handle_line(int fd, char *line) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\r') {
        len--; /* 兼容以 \r\n 结尾的输入 */
        line[len] = '\0';
    }
    /* 协议不允许控制字符。把 Tab 之类换成空格，免得整条消息被服务器拒收。 */
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)line[i];
        if (ch < 0x20 || ch == 0x7F) {
            line[i] = ' ';
        }
    }
    if (len == 0) {
        return INPUT_OK;
    }
    if (line[0] == '/') {
        return run_command(fd, line);
    }
    if (len > PROTO_TEXT_MAX) {
        fprintf(stderr, "message too long (max %d bytes)\n", PROTO_TEXT_MAX);
        return INPUT_OK;
    }
    send_message(fd, MSG_SAY, NULL, line);
    return INPUT_OK;
}

/*
 * 用 read 而不用 fgets 读标准输入：fgets 会让 stdio 一次读进好几行、存在
 * 它自己的缓冲区里，poll 只看内核里有没有数据，看不到 stdio 缓冲区里剩下的
 * 行，那些行就会一直等到下一次有新输入才被处理。
 */
static input_result read_stdin(chat_client *cl) {
    /* 新读到的字节接在上次剩下的半行后面，最多填到缓冲区末尾 */
    char *tail = cl->line + cl->line_len;
    char *limit = cl->line + sizeof cl->line;
    ssize_t n = read(STDIN_FILENO, tail, (size_t)(limit - tail));
    if (n == 0) {
        /* 输入结束。最后一行可能没有换行符（例如 printf '/list' | ...），
         * 也要照常处理；line_len 总小于缓冲区大小，放得下结尾的 '\0'。 */
        if (cl->line_len > 0 && !cl->skipping) {
            cl->line[cl->line_len] = '\0';
            cl->line_len = 0;
            if (handle_line(cl->fd, cl->line) == INPUT_QUIT) {
                return INPUT_QUIT;
            }
        }
        return INPUT_EOF;
    }
    if (n < 0) {
        if (errno == EINTR) {
            return INPUT_OK;
        }
        perror("read");
        return INPUT_EOF;
    }
    cl->line_len += (size_t)n;

    /* 和 TCP 一样，一次 read 得到的不一定正好是一行：
     * 处理所有完整的行，最后的半行留到下一次。 */
    char *start = cl->line;
    char *end = cl->line + cl->line_len;
    char *newline;
    while ((newline = memchr(start, '\n', (size_t)(end - start))) != NULL) {
        *newline = '\0';
        input_result r = cl->skipping ? INPUT_OK : handle_line(cl->fd, start);
        cl->skipping = false;
        start = newline + 1;
        if (r != INPUT_OK) {
            return r;
        }
    }
    size_t rest = (size_t)(end - start);
    if (rest == sizeof cl->line) {
        if (!cl->skipping) {
            fprintf(stderr, "input line too long, ignored\n");
        }
        cl->skipping = true;
        rest = 0;
    }
    memmove(cl->line, start, rest);
    cl->line_len = rest;
    return INPUT_OK;
}

/* 返回 false 表示连接已经结束 */
static bool read_server(chat_client *cl) {
    frame_buffer *in = &cl->in;
    ssize_t n = recv(cl->fd, in->data + in->len, sizeof in->data - in->len, 0);
    if (n == 0) {
        fprintf(stderr, "connection closed by server\n");
        return false;
    }
    if (n < 0) {
        if (errno == EINTR) {
            return true;
        }
        perror("recv");
        return false;
    }
    in->len += (size_t)n;

    message msg;
    proto_status status;
    while ((status = frame_buffer_next(in, &msg)) == PROTO_OK) {
        print_message(&msg);
    }
    if (status != PROTO_INCOMPLETE) {
        fprintf(stderr, "bad data from server: %s\n",
                proto_status_text(status));
        return false;
    }
    return true;
}

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s [-h host] [-p port] NICK\n", prog);
}

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    const char *port = "20000";
    int opt;
    while ((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
        case 'h':
            host = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        default:
            usage(argv[0]);
            return 2;
        }
    }
    if (optind != argc - 1) {
        usage(argv[0]);
        return 2;
    }
    const char *nick = argv[optind];
    if (!nick_is_valid(nick)) {
        fprintf(stderr, "invalid nickname: %s "
                        "(use 1-16 letters, digits, '_' or '-')\n",
                nick);
        return 2;
    }

    /* 标准输出接到文件或管道时是全缓冲的，收到的消息会积在 stdio 缓冲区里
     * 迟迟不出现。改成行缓冲，每打印完一行就交给内核；集成测试正是通过
     * 读取客户端的输出文件来检查结果的。 */
    setvbuf(stdout, NULL, _IOLBF, 0);
    /* 服务器断开后再 send 会产生 SIGPIPE，默认动作是终止进程。忽略它，
     * 让 send 返回 EPIPE，再由 recv 读到连接结束、正常退出。 */
    signal(SIGPIPE, SIG_IGN);

    chat_client cl = {.fd = net_connect(host, port)};
    if (cl.fd < 0) {
        return 1;
    }
    fprintf(stderr, "connected to %s:%s, type /help for commands\n", host,
            port);
    send_message(cl.fd, MSG_NICK, nick, NULL);

    struct pollfd fds[2] = {
        {.fd = STDIN_FILENO, .events = POLLIN},
        {.fd = cl.fd, .events = POLLIN},
    };
    for (;;) {
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }
        if (fds[1].revents != 0 && !read_server(&cl)) {
            break;
        }
        if (fds[0].revents != 0) {
            input_result r = read_stdin(&cl);
            if (r == INPUT_QUIT) {
                break;
            }
            if (r == INPUT_EOF) {
                /* 输入结束（Ctrl-D 或管道关闭）：半关闭连接，告诉服务器
                 * “我不会再发了”，但继续接收，直到服务器关闭连接。这样
                 * printf '/list\n' | chat_client alice 也能看到回复。
                 * 把 fd 设为负数，poll 就不再关注这一项。 */
                shutdown(cl.fd, SHUT_WR);
                fds[0].fd = -1;
            }
        }
    }
    close(cl.fd);
    return 0;
}
