#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHM_FILE "share.dat"
#define SHM_SIZE 4096

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd == -1) {
        die("open");
    }
    if (ftruncate(fd, SHM_SIZE) == -1) {
        die("ftruncate");
    }
    char *p = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        die("mmap");
    }
    strcpy(p, "writer 写于某个时刻\n");
    printf("写入：%s", p);
    if (msync(p, SHM_SIZE, MS_SYNC) == -1) {
        die("msync");
    }
    munmap(p, SHM_SIZE);
    close(fd);
    return EXIT_SUCCESS;
}
