#ifndef OPENWATCH_H
#define OPENWATCH_H

#define TASK_COMM_LEN 16
#define PATH_LEN 256

/* 入口程序记下、出口程序取回的参数 */
struct open_args {
    const char *filename;
    __s64 flags;
};

/* 送到用户态的事件 */
struct event {
    __u64 ts;
    __u32 pid;
    __u32 tid;
    __u32 uid;
    __s32 ret;
    __s64 flags;
    char comm[TASK_COMM_LEN];
    char path[PATH_LEN];
};

#endif
