#include <stdio.h>
#include <windows.h>

/* 只导出一个无关函数，用来冒充 version.dll。 */
__declspec(dllexport) int fake_version_dll(void) {
    return 1;
}