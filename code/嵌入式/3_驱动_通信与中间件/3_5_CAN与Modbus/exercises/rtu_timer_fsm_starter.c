#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * 练习 63340 初始代码：
 * Modbus RTU 接收定时状态机（支持 t1.5 字符间超时检测与 t3.5 帧结束定界）
 */
enum rtu_rx_state {
    RTU_STATE_IDLE = 0,
    RTU_STATE_RECEIVING,
    RTU_STATE_FRAME_NOK, /* 帧内字符间隔超过 t1.5，当前帧作废，等待 t3.5 静默结束 */
};

struct rtu_rx_fsm {
    enum rtu_rx_state state;
    uint8_t buf[256];
    size_t len;
};

void rtu_fsm_init(struct rtu_rx_fsm *fsm) {
    fsm->state = RTU_STATE_IDLE;
    fsm->len = 0u;
}

/* 收到一个串口字节时调用（同时相当于重启 t1.5 与 t3.5 定时器） */
void rtu_fsm_on_byte(struct rtu_rx_fsm *fsm, uint8_t byte) {
    /* TODO: 根据当前状态处理新到达的字节 */
    (void)fsm;
    (void)byte;
}

/* 当静默时间达到 t1.5（但尚未达到 t3.5）且紧接着又有字节到达前触发 */
void rtu_fsm_on_t15_expired(struct rtu_rx_fsm *fsm) {
    /* TODO: 标记后续若在 t3.5 之前再收到字节则判为断帧 */
    (void)fsm;
}

/* 当静默时间达到 t3.5 时调用；若收到了完整合法的一帧则返回帧长度，否则返回 0 */
size_t rtu_fsm_on_t35_expired(struct rtu_rx_fsm *fsm) {
    /* TODO: 结算当前帧并回到 IDLE */
    (void)fsm;
    return 0u;
}

int main(void) {
    struct rtu_rx_fsm fsm;
    rtu_fsm_init(&fsm);

    /* 正常连续到达 4 个字节，随后 t3.5 到期 */
    rtu_fsm_on_byte(&fsm, 0x01u);
    rtu_fsm_on_byte(&fsm, 0x03u);
    rtu_fsm_on_byte(&fsm, 0x00u);
    rtu_fsm_on_byte(&fsm, 0x00u);
    printf("Normal frame len = %u\n", (unsigned)rtu_fsm_on_t35_expired(&fsm));
    return 0;
}
