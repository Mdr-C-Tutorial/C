#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

extern char end[];

void *_sbrk(ptrdiff_t incr) {
    static char *heap_end;
    if (heap_end == NULL) {
        heap_end = end;
    }
    char *prev_heap_end = heap_end;
    heap_end += incr;
    return prev_heap_end;
}

int _write(int fd, const void *buf, size_t count) {
    (void)fd;
    (void)buf;
    return (int)count;
}

int _read(int fd, void *buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

int _close(int fd) {
    (void)fd;
    return -1;
}

int _fstat(int fd, struct stat *st) {
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd) {
    (void)fd;
    return 1;
}

int _lseek(int fd, int ptr, int dir) {
    (void)fd;
    (void)ptr;
    (void)dir;
    return 0;
}

void _exit(int status) {
    (void)status;
    for (;;) {
    }
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void) {
    return 1;
}
