#ifndef MDRPLUGIN_H
#define MDRPLUGIN_H

/* 插件 DLL 必须导出的两个函数。只使用 C 类型。 */
#define MDR_PLUGIN_ENTRY "plugin_name"
#define MDR_PLUGIN_RUN "plugin_run"

typedef const char *(*plugin_name_fn)(void);
typedef int (*plugin_run_fn)(int);

#endif