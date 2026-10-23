#ifndef PLATFORM_H
#define PLATFORM_H

/* 标准输出是否接在终端上：接终端返回 1，重定向到文件或管道时返回 0 */
int wf_stdout_is_terminal(void);

#endif
