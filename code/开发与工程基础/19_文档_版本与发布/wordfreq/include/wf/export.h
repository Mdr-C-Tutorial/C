/* SPDX-License-Identifier: MIT */
/*
 * wf 库的导出标记：共享库在 Windows 上默认不导出符号，
 * 需要 __declspec(dllexport) 声明哪些是公开接口。
 *
 * - 构建 wf 自身时定义 WF_BUILDING；
 * - 使用共享库时定义 WF_SHARED；
 * - 静态库构建两者都不需要，WF_API 展开为空。
 */
#ifndef WF_EXPORT_H
#define WF_EXPORT_H

#if defined(WF_SHARED) && defined(_WIN32)
#ifdef WF_BUILDING
#define WF_API __declspec(dllexport)
#else
#define WF_API __declspec(dllimport)
#endif
#elif defined(WF_SHARED) && defined(__GNUC__)
#define WF_API __attribute__((visibility("default")))
#else
#define WF_API
#endif

#endif
