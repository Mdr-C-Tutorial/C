# 约瑟夫环

## 题目

n 个人围成圈，报偶数的人出列，求最后剩下的人的编号。

## 解析

这是经典的约瑟夫环问题的变体。解题思路：

1. 用数组记录每个人的状态（在圈中/已出列）
2. 模拟报数过程，遇到偶数则标记出列
3. 直到只剩一人为止

## 答案

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int n;
    printf("输入人数：");
    scanf("%d", &n);

    int* alive = calloc(n + 1, sizeof(int));  // 0 表示在圈中
    int remain = n;  // 剩余人数
    int count = 1;   // 报数
    int pos = 1;     // 当前位置

    while(remain > 1) {
        if(!alive[pos]) {  // 如果这个人还在圈中
            if(count % 2 == 0) {  // 报到偶数
                alive[pos] = 1;   // 标记出列
                remain--;
            }
            count++;
        }
        pos = pos % n + 1;  // 移到下一个位置
    }

    // 找到最后剩下的人
    for(int i = 1; i <= n; i++) {
        if(!alive[i]) {
            printf("最后剩下：%d\n", i);
            break;
        }
    }

    free(alive);
    return 0;
}
```
