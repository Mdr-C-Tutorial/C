/* raw_load.c：不依赖 libbpf，直接用 bpf() 系统调用建 map、加载并附加一个套接字过滤器。
 *
 * 编译：gcc -std=c17 -Wall -Wextra raw_load.c -o raw_load
 * 运行：sudo ./raw_load（需要 root，或 CAP_BPF 与 CAP_NET_ADMIN）
 *
 * 程序把一个返回 0 的套接字过滤器挂在本进程绑定于 127.0.0.1:20003 的 UDP 套接字上。
 * 套接字过滤器返回 0 表示丢弃报文，所以此后发给这个套接字的数据报都收不到；
 * 过滤器每运行一次就把 map 里的计数加一，用户态再把它读出来。
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE /* syscall() 不在 POSIX 里，需要 GNU 扩展的声明 */

#include <arpa/inet.h>
#include <errno.h>
#include <linux/bpf.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#define LISTEN_PORT 20003
#define DATAGRAM_COUNT 3

/* bpf() 没有 glibc 的包装函数，libbpf 内部也是直接调 syscall()。
 * 第三个参数是 union bpf_attr 的大小，内核按它检查调用者认识的结构有多新。 */
static int bpf_sys(enum bpf_cmd cmd, union bpf_attr *attr)
{
    return (int)syscall(__NR_bpf, cmd, attr, sizeof(*attr));
}

static int create_array_map(void)
{
    union bpf_attr attr;

    /* 没用到的字段必须清零，内核会把非零的保留字段当作错误参数拒绝。 */
    memset(&attr, 0, sizeof(attr));
    attr.map_type = BPF_MAP_TYPE_ARRAY;
    attr.key_size = sizeof(__u32);
    attr.value_size = sizeof(__u32);
    attr.max_entries = 1;
    return bpf_sys(BPF_MAP_CREATE, &attr);
}

/* 套接字过滤器的字节码。libbpf 从 C 源码生成这样的指令数组，
 * 这里手写一遍，看清 BPF_PROG_LOAD 到底收到了什么。
 * 语义：value = map[0]; value++; map[0] = value; return 0; */
static struct bpf_insn prog[] = {
    /* 槽 0～1：r1 = map 的 fd。lddw 占两个槽，第二个槽必须全零。
     * imm 字段在加载前填入，见 load_prog()。 */
    {.code = BPF_LD | BPF_DW | BPF_IMM,
     .dst_reg = BPF_REG_1,
     .src_reg = BPF_PSEUDO_MAP_FD,
     .off = 0,
     .imm = 0},
    {.code = 0, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0},
    /* 2：*(u32 *)(r10 - 4) = 0，键 0 放在栈上。r10 是只读的帧指针。 */
    {.code = BPF_ST | BPF_MEM | BPF_W, .dst_reg = BPF_REG_10, .off = -4, .imm = 0},
    /* 3：r2 = r10，r2 用来传键的地址。 */
    {.code = BPF_ALU64 | BPF_MOV | BPF_X, .dst_reg = BPF_REG_2, .src_reg = BPF_REG_10},
    /* 4：r2 += -4，键的地址是 r10 - 4。 */
    {.code = BPF_ALU64 | BPF_ADD | BPF_K, .dst_reg = BPF_REG_2, .imm = -4},
    /* 5：call bpf_map_lookup_elem，返回值在 r0。 */
    {.code = BPF_JMP | BPF_CALL, .imm = BPF_FUNC_map_lookup_elem},
    /* 6：if (r0 == NULL) goto 9，偏移 2 跳过下面的加一。 */
    {.code = BPF_JMP | BPF_JEQ | BPF_K, .dst_reg = BPF_REG_0, .off = 2, .imm = 0},
    /* 7：r1 = 1。 */
    {.code = BPF_ALU64 | BPF_MOV | BPF_K, .dst_reg = BPF_REG_1, .imm = 1},
    /* 8：lock *(u32 *)(r0 + 0) += r1，原子地加一。 */
    {.code = BPF_STX | BPF_XADD | BPF_W,
     .dst_reg = BPF_REG_0,
     .src_reg = BPF_REG_1,
     .off = 0},
    /* 9：r0 = 0，套接字过滤器返回保留的字节数，0 表示丢弃。 */
    {.code = BPF_ALU64 | BPF_MOV | BPF_K, .dst_reg = BPF_REG_0, .imm = 0},
    /* 10：exit。 */
    {.code = BPF_JMP | BPF_EXIT},
};

static int load_prog(int map_fd)
{
    /* 验证器日志可以很长，放在静态区，不要占用栈。 */
    static char log_buf[64 * 1024];
    static const char license[] = "GPL";
    union bpf_attr attr;
    int fd;

    /* libbpf 在加载前做的事之一就是这一步：把重定位指向的 map fd 填进指令。 */
    prog[0].imm = map_fd;

    memset(&attr, 0, sizeof(attr));
    attr.prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
    attr.insn_cnt = sizeof(prog) / sizeof(prog[0]);
    attr.insns = (unsigned long)prog;
    attr.license = (unsigned long)license;

    /* 第一次不带日志：log_level 为 0 时 log_buf 必须是 NULL、log_size 必须是 0。
     * 失败后打开日志重试一次，这跟 libbpf 的做法相同。 */
    fd = bpf_sys(BPF_PROG_LOAD, &attr);
    if (fd >= 0) {
        return fd;
    }
    if (errno == EPERM || errno == EACCES) {
        fprintf(stderr, "BPF_PROG_LOAD 失败：%s（需要 root 或 CAP_BPF）\n", strerror(errno));
        return -1;
    }

    attr.log_level = 1;
    attr.log_buf = (unsigned long)log_buf;
    attr.log_size = sizeof(log_buf);
    fd = bpf_sys(BPF_PROG_LOAD, &attr);
    if (fd < 0) {
        fprintf(stderr, "BPF_PROG_LOAD 失败：%s\n验证器日志：\n%s\n", strerror(errno), log_buf);
        return -1;
    }
    return fd;
}

static int lookup_count(int map_fd)
{
    union bpf_attr attr;
    __u32 key = 0;
    __u32 value = 0;

    memset(&attr, 0, sizeof(attr));
    attr.map_fd = map_fd;
    attr.key = (unsigned long)&key;
    attr.value = (unsigned long)&value;
    if (bpf_sys(BPF_MAP_LOOKUP_ELEM, &attr) < 0) {
        fprintf(stderr, "BPF_MAP_LOOKUP_ELEM 失败：%s\n", strerror(errno));
        return -1;
    }
    return (int)value;
}

int main(void)
{
    struct sockaddr_in addr;
    char buf[64];
    int sock;
    int map_fd;
    int prog_fd;
    ssize_t n;
    int i;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    map_fd = create_array_map();
    if (map_fd < 0) {
        fprintf(stderr, "BPF_MAP_CREATE 失败：%s\n", strerror(errno));
        return 1;
    }
    prog_fd = load_prog(map_fd);
    if (prog_fd < 0) {
        return 1;
    }

    /* 附加：套接字一直存在，过滤器就一直挂着；close(sock) 或进程退出时一并消失。 */
    if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_BPF, &prog_fd, sizeof(prog_fd)) < 0) {
        perror("setsockopt(SO_ATTACH_BPF)");
        return 1;
    }

    for (i = 0; i < DATAGRAM_COUNT; i++) {
        if (sendto(sock, "hello", 5, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("sendto");
            return 1;
        }
    }

    n = recv(sock, buf, sizeof(buf), MSG_DONTWAIT);
    if (n < 0) {
        printf("recv 失败：errno = %d（%s）\n", errno, strerror(errno));
    } else {
        printf("recv 收到 %zd 字节\n", n);
    }
    printf("过滤器运行了 %d 次\n", lookup_count(map_fd));

    close(prog_fd);
    close(map_fd);
    close(sock);
    return 0;
}