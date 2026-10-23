/* 演示 exec 前用 close_range 清掉继承的描述符。 */
#define _GNU_SOURCE /* close_range */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    int fd = open("data.txt", O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    printf("opened fd %d\n", fd);
    fflush(stdout);
    close_range(3, ~0U, 0);
    execl("./fdlist", "fdlist", (char *)NULL);
    perror("execl");
    return 1;
}