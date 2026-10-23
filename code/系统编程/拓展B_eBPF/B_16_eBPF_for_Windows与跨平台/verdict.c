/* 打印 ebpf_sock_addr_verdict_t 三个枚举值的实际取值。
 *
 *   gcc -std=c17 -Wall -Wextra -I<ebpf-for-windows>\include verdict.c -o verdict.exe
 *
 * 先包含 stdint.h 与 stddef.h：ebpf_windows.h 用 uint8_t 和 size_t，
 * 但在 GCC 下不会自己包含这两个头。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ebpf_nethooks.h"

int main(void) {
    printf("REJECT        = %d\n", (int)BPF_SOCK_ADDR_VERDICT_REJECT);
    printf("PROCEED_SOFT  = %d\n", (int)BPF_SOCK_ADDR_VERDICT_PROCEED_SOFT);
    printf("PROCEED_HARD  = %d\n", (int)BPF_SOCK_ADDR_VERDICT_PROCEED_HARD);
    return 0;
}
