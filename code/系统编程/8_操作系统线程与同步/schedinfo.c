#define _GNU_SOURCE

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>

static const char *policy_name(int policy) {
    switch (policy) {
    case SCHED_OTHER:
        return "SCHED_OTHER";
    case SCHED_FIFO:
        return "SCHED_FIFO";
    case SCHED_RR:
        return "SCHED_RR";
    case SCHED_BATCH:
        return "SCHED_BATCH";
    case SCHED_IDLE:
        return "SCHED_IDLE";
    default:
        return "unknown";
    }
}

int main(void) {
    int policy = sched_getscheduler(0);
    struct sched_param sp;
    cpu_set_t set;

    if (sched_getparam(0, &sp) != 0) {
        perror("sched_getparam");
        return 1;
    }
    errno = 0;
    int nice_value = getpriority(PRIO_PROCESS, 0);
    if (nice_value == -1 && errno != 0) {
        perror("getpriority");
        return 1;
    }
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof set, &set) != 0) {
        perror("sched_getaffinity");
        return 1;
    }
    printf("policy = %s, rt_priority = %d, nice = %d\n", policy_name(policy),
           sp.sched_priority, nice_value);
    printf("allowed CPUs = %d, running on CPU %d\n", CPU_COUNT(&set), sched_getcpu());
    return 0;
}