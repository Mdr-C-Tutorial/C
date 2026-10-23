#!/usr/bin/env python3
"""mkstack.py：按真实 ELF 里的返回地址构造一份栈转储，供 stackscan 演示。

输入是构造的，stackscan 的输出是真实的。每个地址都取自 fw.elf 里
bl 指令之后那条指令的地址（最低位必须为 1）。
"""
import os
import struct

# 异常入栈的 8 个字，后面是三层调用保存的 LR，最后两个是干扰项
WORDS = [
    0x00000000,  # r0
    0x20000400,  # r1
    0x00000018,  # r2
    0x00000000,  # r3
    0x00000000,  # r12
    0x0800013F,  # lr：sensor_poll 里 bl protocol_write 之后
    0x080000EC,  # pc：故障指令
    0x61000000,  # xpsr
    0x20000418,  # 局部对象
    0x08000153,  # sensor_poll 保存的 lr：app_tick 里 bl 之后
    0x20000420,  # 局部对象
    0x08000167,  # app_tick 保存的 lr：main 里 bl 之后
    0x00000002,
    0x080001A3,  # main 保存的 lr：Reset_Handler 里 bl main 之后
    0xCCCCCCCC,  # 未初始化数据，不是返回地址
    0x080000ED,  # 碰巧落在 .text 里、最低位为 1 的干扰项
]


def main():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stack.bin")
    with open(path, "wb") as f:
        for w in WORDS:
            f.write(struct.pack("<I", w))
    print("wrote", len(WORDS), "words to", path)


if __name__ == "__main__":
    main()