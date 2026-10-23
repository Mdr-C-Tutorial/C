#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void show(const char *name) {
    struct stat st;
    if (stat(name, &st) == -1) {
        die("stat");
    }
    printf("%-16s %04o\n", name, (unsigned)(st.st_mode & 07777));
}

int main(void) {
    mode_t old = umask(022);
    printf("umask 022 (was %04o)\n", (unsigned)old);

    int fd = open("u_file.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fd == -1) {
        die("open");
    }
    if (close(fd) == -1) {
        die("close");
    }
    if (mkdir("u_dir", 0777) == -1) {
        die("mkdir");
    }
    show("u_file.txt");
    show("u_dir");

    umask(077);
    printf("umask 077\n");
    fd = open("v_file.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fd == -1) {
        die("open");
    }
    if (close(fd) == -1) {
        die("close");
    }
    if (mkdir("v_dir", 0777) == -1) {
        die("mkdir");
    }
    show("v_file.txt");
    show("v_dir");

    if (chmod("u_file.txt", 0640) == -1) {
        die("chmod");
    }
    printf("after chmod 0640:\n");
    show("u_file.txt");
    return 0;
}
