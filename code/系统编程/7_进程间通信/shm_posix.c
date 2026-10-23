#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHM_NAME "/p087_demo"
#define SHM_SIZE 4096

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    /* 名字形如 /path，对象出现在 /dev/shm 下 */
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd == -1) {
        die("shm_open");
    }
    if (ftruncate(fd, SHM_SIZE) == -1) {
        die("ftruncate");
    }

    char *p = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        die("mmap");
    }
    snprintf(p, SHM_SIZE, "另一个进程用同样的名字就能看到这一行");

    /* 另一个进程：shm_open(SHM_NAME, O_RDWR, 0) + mmap 后读 p */

    munmap(p, SHM_SIZE);
    close(fd);
    if (shm_unlink(SHM_NAME) == -1) {
        die("shm_unlink");
    }
    return EXIT_SUCCESS;
}
