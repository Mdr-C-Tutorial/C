/* SRW 锁被持有时 TryAcquire 的结果。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    SRWLOCK lock = SRWLOCK_INIT;

    AcquireSRWLockExclusive(&lock);
    printf("exclusive held: TryAcquireExclusive -> %d\n",
           TryAcquireSRWLockExclusive(&lock));
    printf("exclusive held: TryAcquireShared    -> %d\n", TryAcquireSRWLockShared(&lock));
    ReleaseSRWLockExclusive(&lock);

    AcquireSRWLockShared(&lock);
    printf("shared held:    TryAcquireExclusive -> %d\n",
           TryAcquireSRWLockExclusive(&lock));
    ReleaseSRWLockShared(&lock);

    printf("free:           TryAcquireExclusive -> %d\n",
           TryAcquireSRWLockExclusive(&lock));
    ReleaseSRWLockExclusive(&lock);
    return 0;
}