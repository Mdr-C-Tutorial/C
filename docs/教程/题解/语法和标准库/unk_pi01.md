# 投飞镖估算π

## 题目

“投飞镖”可以近似 $\pi$ 的值。取 $[-1,1]\times[-1,1]$ 的区域，向其中随机投掷 M 个飞镖，会产生 M 个落点。若其中有 m 个落点落在单位圆内，则 $\pi$ 的近似值为 $\frac{4m}{M}$。设计程序，输入投掷次数，估算 $\pi$ 的值。

## 解析

这是蒙特卡洛方法的典型应用。原理：

1. 在正方形区域内随机投掷点，正方形边长为 2，中心在原点。
2. 统计落在单位圆内的点数。
3. $\pi$ 值近似等于：$\pi\approx\frac{4N_{\text{圆内}}}{N_{\text{总}}}$

这是因为单位圆与其外接正方形的面积比为 $\frac{\pi r^2}{(2r)^2}=\frac{\pi}{4}$。当采样点数量足够大时，点的分布比例会近似于面积比。

## 答案

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    int M;
    printf("输入投掷次数：");
    scanf("%d", &M);
    
    srand(time(NULL));
    int in_circle = 0;
    
    for(int i = 0; i < M; i++) {
        // 使用 -1 到 1 的范围以充分利用圆形区域
        double x = 2.0 * rand() / RAND_MAX - 1.0;
        double y = 2.0 * rand() / RAND_MAX - 1.0;
        if(x*x + y*y <= 1) {
            in_circle++;
        }
    }
    
    double pi = 4.0 * in_circle / M;
    printf("π ≈ %.6f\n", pi);
    return 0;
}
```
