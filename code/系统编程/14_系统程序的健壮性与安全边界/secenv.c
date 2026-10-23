/* secure_getenv：setuid 程序里应当用它代替 getenv。
 * 普通用户运行时两者结果相同；AT_SECURE 为 1（setuid/setgid 或
 * 带文件能力的程序）时它返回 NULL，避免被调用者的环境左右。
 * 这里只演示两者在普通运行下的一致性，并打印 AT_SECURE。 */
#define _GNU_SOURCE /* secure_getenv、getauxval */
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>

int main(void) {
    const char *name = "PATH";
    printf("AT_SECURE = %lu\n", (unsigned long)getauxval(AT_SECURE));
    printf("getenv        = %s\n", getenv(name) ? "set" : "(null)");
    printf("secure_getenv = %s\n", secure_getenv(name) ? "set" : "(null)");
    return 0;
}