#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;

    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

/* 把 data 原子地写到 path：临时文件写完、fsync、rename，再同步目录。 */
static void atomic_save(const char *path, const char *data, size_t len) {
    size_t tmplen = strlen(path) + 8;
    char *tmp = malloc(tmplen);
    if (tmp == NULL) {
        die("malloc");
    }
    snprintf(tmp, tmplen, "%s.XXXXXX", path);

    int fd = mkstemp(tmp);
    if (fd == -1) {
        die("mkstemp");
    }
    /* mkstemp 建的文件固定是 0600；目标已存在就沿用它的权限。 */
    struct stat st;
    if (stat(path, &st) == 0) {
        if (fchmod(fd, st.st_mode & 07777) == -1) {
            die("fchmod");
        }
    } else if (errno == ENOENT) {
        if (fchmod(fd, 0644) == -1) {
            die("fchmod");
        }
    } else {
        die("stat");
    }
    if (write_all(fd, data, len) == -1) {
        die("write");
    }
    if (fsync(fd) == -1) {
        die("fsync");
    }
    if (close(fd) == -1) {
        die("close");
    }
    if (rename(tmp, path) == -1) {
        die("rename");
    }
    free(tmp);

    /* rename 只保证目录项变化是原子的；要让新名字也落盘，
       还要把所在的目录 fsync 一次。 */
    char *dirpath = strdup(path);
    if (dirpath == NULL) {
        die("strdup");
    }
    char *slash = strrchr(dirpath, '/');
    if (slash == NULL) {
        strcpy(dirpath, ".");
    } else if (slash == dirpath) {
        dirpath[1] = '\0';
    } else {
        *slash = '\0';
    }
    int dfd = open(dirpath, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd == -1) {
        die("open dir");
    }
    if (fsync(dfd) == -1) {
        die("fsync dir");
    }
    if (close(dfd) == -1) {
        die("close dir");
    }
    free(dirpath);
}

/* 读入整个文件，返回长度；文件不存在时返回 0。 */
static size_t read_file(const char *path, char **out) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        if (errno == ENOENT) {
            *out = malloc(1);
            if (*out == NULL) {
                die("malloc");
            }
            (*out)[0] = '\0';
            return 0;
        }
        die("open");
    }

    size_t cap = 256;
    size_t len = 0;
    char *buf = malloc(cap);
    if (buf == NULL) {
        die("malloc");
    }
    for (;;) {
        if (len == cap) {
            cap *= 2;
            char *p = realloc(buf, cap);
            if (p == NULL) {
                die("realloc");
            }
            buf = p;
        }
        ssize_t n = read(fd, buf + len, cap - len);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("read");
        }
        if (n == 0) {
            break;
        }
        len += (size_t)n;
    }
    if (close(fd) == -1) {
        die("close");
    }
    *out = buf;
    return len;
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : "config.txt";

    if (argc <= 2) {
        const char *initial = "mode = fast\nretries = 3\n";
        atomic_save(path, initial, strlen(initial));
        printf("saved %s\n", path);
        return EXIT_SUCCESS;
    }

    /* 读入旧内容，追加一行，再整体原子替换。 */
    char *old;
    size_t oldlen = read_file(path, &old);
    size_t addlen = strlen(argv[2]);
    char *combined = malloc(oldlen + addlen + 2);
    if (combined == NULL) {
        die("malloc");
    }
    memcpy(combined, old, oldlen);
    size_t len = oldlen;
    if (len > 0 && combined[len - 1] != '\n') {
        combined[len++] = '\n';
    }
    memcpy(combined + len, argv[2], addlen);
    len += addlen;
    combined[len++] = '\n';

    atomic_save(path, combined, len);
    printf("saved %s\n", path);
    free(old);
    free(combined);
    return EXIT_SUCCESS;
}
