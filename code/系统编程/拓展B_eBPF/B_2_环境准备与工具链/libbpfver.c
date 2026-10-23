#define _POSIX_C_SOURCE 200809L

#include <bpf/libbpf.h>
#include <stdio.h>

int main(void) {
    printf("libbpf %s\n", libbpf_version_string());
    return 0;
}
