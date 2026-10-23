/* SRW 锁不可递归：同一线程重复获取独占锁会一直等下去。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    SRWLOCK lock = SRWLOCK_INIT;

    AcquireSRWLockExclusive(&lock);
    puts("first acquire done");
    fflush(stdout);
    AcquireSRWLockExclusive(&lock);
    puts("second acquire done — this line never prints");
    ReleaseSRWLockExclusive(&lock);
    ReleaseSRWLockExclusive(&lock);
    return 0;
}