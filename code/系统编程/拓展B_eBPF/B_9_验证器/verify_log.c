#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <bpf/libbpf.h>

static int print_libbpf(enum libbpf_print_level level, const char *format,
                        va_list args)
{
    (void)level;
    return vfprintf(stderr, format, args);
}

int main(int argc, char **argv)
{
    struct bpf_object *obj;
    struct bpf_program *prog;
    int err;

    if (argc != 2) {
        fprintf(stderr, "用法: %s 目标文件.bpf.o\n", argv[0]);
        return 1;
    }

    libbpf_set_print(print_libbpf);

    obj = bpf_object__open_file(argv[1], NULL);
    err = libbpf_get_error(obj);
    if (err != 0) {
        fprintf(stderr, "打开 %s 失败：%s\n", argv[1], strerror(-err));
        return 1;
    }

    bpf_object__for_each_program(prog, obj)
    {
        fprintf(stderr, "加载程序 %s\n", bpf_program__name(prog));
        bpf_program__set_log_level(prog, 1);
    }

    err = bpf_object__load(obj);
    if (err != 0) {
        fprintf(stderr, "加载失败：%s\n", strerror(-err));
        bpf_object__close(obj);
        return 1;
    }

    printf("%s 通过验证\n", argv[1]);
    bpf_object__close(obj);
    return 0;
}