#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHM_FILE "share.dat"

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    int fd = open(SHM_FILE, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        die("open");
    }
    char *p = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        die("mmap");
    }
    printf("读到：%s", p);
    munmap(p, 4096);
    close(fd);
    return EXIT_SUCCESS;
}
