#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    /* 第一种：返回 -1，原因在 errno 里 */
    errno = 0;
    if (write(-1, "x", 1) == -1) {
        printf("write: %s\n", strerror(errno));
    }

    /* 第二种：直接返回错误号，不设置 errno */
    errno = 0;
    int rc = pthread_join(pthread_self(), NULL);
    if (rc != 0) {
        printf("pthread_join: %s（errno 还是 %d）\n", strerror(rc), errno);
    }

    /* 第三种：返回专用错误码，用 gai_strerror 解释 */
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;
    rc = getaddrinfo(NULL, NULL, &hints, &res);
    if (rc != 0) {
        printf("getaddrinfo: %s（rc = %d）\n", gai_strerror(rc), rc);
    } else {
        freeaddrinfo(res);
    }
    return 0;
}
