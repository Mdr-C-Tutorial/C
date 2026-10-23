#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TARGET "lockme.dat"

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int try_lock(int fd, int cmd) {
    struct flock fl = {0};
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1; /* 只锁第一个字节 */
    return fcntl(fd, cmd, &fl);
}

int main(int argc, char *argv[]) {
    int use_ofd = argc > 1 && strcmp(argv[1], "ofd") == 0;
    int setlk = use_ofd ? F_OFD_SETLK : F_SETLK;

    int a = open(TARGET, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (a == -1) {
        die("open a");
    }
    if (write(a, "x", 1) != 1) {
        die("write");
    }
    if (try_lock(a, setlk) == -1) {
        die("first lock");
    }
    printf("用描述符 %d 拿到锁\n", a);

    /* 同一个文件再打开一次，模拟库里顺手 open/close 的代码 */
    int b = open(TARGET, O_RDWR | O_CLOEXEC);
    if (b == -1) {
        die("open b");
    }
    printf("另开描述符 %d，现在把它关掉\n", b);
    close(b);

    /* 另一个进程能不能拿到锁？这里用同一个进程再开一个描述符判断：
       fcntl 锁属于进程，同一进程的第二个描述符不会冲突，所以让子进程试 */
    fflush(stdout); /* 子进程不要重复输出父进程留在缓冲区里的行 */
    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        close(a); /* 子进程不需要继承来的描述符 */
        int c = open(TARGET, O_RDWR | O_CLOEXEC);
        if (c == -1) {
            die("open c");
        }
        int rc = try_lock(c, setlk);
        if (rc == -1) {
            printf("子进程：拿不到锁（%s）\n", strerror(errno));
            fflush(stdout);
            _exit(1);
        }
        printf("子进程：拿到了锁\n");
        fflush(stdout);
        _exit(0);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        die("waitpid");
    }
    printf("父进程：子进程结束，退出码 %d\n",
           WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    close(a);
    return 0;
}
