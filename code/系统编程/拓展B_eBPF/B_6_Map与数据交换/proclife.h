#ifndef PROCLIFE_H
#define PROCLIFE_H

#define TASK_COMM_LEN 16
#define MAX_FILENAME_LEN 127

/* 内核态程序与用户态程序共用的事件布局 */
struct event {
    __u32 pid;
    __u32 ppid;
    __u32 exit_code;
    __u32 exit_event;
    __u64 duration_ns;
    char comm[TASK_COMM_LEN];
    char filename[MAX_FILENAME_LEN];
};

#endif
