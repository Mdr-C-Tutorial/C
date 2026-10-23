#ifndef MDRMATH_H
#define MDRMATH_H

/*
 * 构建 DLL 时定义 MDRMATH_BUILD，函数标成 dllexport；
 * 使用 DLL 时不定义，函数标成 dllimport。
 */
#ifdef MDRMATH_BUILD
#define MDR_API __declspec(dllexport)
#else
#define MDR_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

MDR_API int mdr_add(int a, int b);
MDR_API char *mdr_greet(const char *name);
MDR_API void mdr_buffer_free(void *block);

#ifdef __cplusplus
}
#endif

#endif