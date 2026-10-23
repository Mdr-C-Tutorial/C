/*
 * idle_stub.c：空闲钩子的空实现。
 *
 * 公共配置打开了 configUSE_IDLE_HOOK，内核要求每个工程都提供
 * vApplicationIdleHook()。不关心空闲时间的示例链接这一份空实现，
 * idle_hook 示例则用自己的实现。
 */
void vApplicationIdleHook(void) {
}
