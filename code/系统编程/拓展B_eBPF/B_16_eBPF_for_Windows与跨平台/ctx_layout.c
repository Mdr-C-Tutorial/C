#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ebpf_nethooks.h"

/* Linux 侧：vmlinux.h 中 struct bpf_sock_addr 的字段，按原样抄录。 */
struct bpf_sock_addr_linux {
    uint32_t user_family;
    uint32_t user_ip4;
    uint32_t user_ip6[4];
    uint32_t user_port;
    uint32_t family;
    uint32_t type;
    uint32_t protocol;
    uint32_t msg_src_ip4;
    uint32_t msg_src_ip6[4];
    void *sk;
};

#define SHOW(label, type)                                                                                          \
    printf("%-8s %-16s size %2lu  user_ip4 @ %2lu  user_port @ %2lu  protocol @ %2lu\n", label, #type,               \
           (unsigned long)sizeof(type), (unsigned long)offsetof(type, user_ip4),                                   \
           (unsigned long)offsetof(type, user_port), (unsigned long)offsetof(type, protocol))

int main(void) {
    SHOW("Linux", struct bpf_sock_addr_linux);
    SHOW("Windows", bpf_sock_addr_t);
    return 0;
}
