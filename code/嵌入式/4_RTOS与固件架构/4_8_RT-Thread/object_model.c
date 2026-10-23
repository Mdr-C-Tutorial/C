/*
 * object_model.c：把 rt_thread 指针当 rt_object 用，读对象名与类型；
 * 再用 rt_object_for_each 遍历所有线程对象。
 *
 * 只看编译与符号，不链接成固件。
 */
#include <rtthread.h>

static void dump_object(rt_object_t obj) {
    char name[RT_NAME_MAX + 1] = { 0 };

    rt_object_get_name(obj, name, sizeof(name));
    rt_kprintf("object name=%-8s type=%d\n", name, (int)rt_object_get_type(obj));
}

static rt_err_t print_thread(rt_object_t object, void *data) {
    rt_uint32_t *count = (rt_uint32_t *)data;

    dump_object(object);
    (*count)++;
    return RT_EOK;
}

int main(void) {
    rt_thread_t tid = rt_thread_create("worker", RT_NULL, RT_NULL, 1024, 10, 10);
    rt_uint32_t count = 0;

    if (tid == RT_NULL) {
        return 0;
    }

    /* rt_thread 的第一个成员就是 struct rt_object，指针可以直接互转 */
    dump_object((rt_object_t)tid);

    if (rt_object_for_each(RT_Object_Class_Thread, print_thread, &count) == RT_EOK) {
        rt_kprintf("thread objects: %u\n", (unsigned)count);
    }

    rt_thread_delete(tid);
    return 0;
}