# 投针估算π

## 题目

“投针”可以近似 $\pi$ 的值。在无限大的桌面上设置间距为 d 的一组平行线。将 M 个长度为 d 的针随机投掷到桌面上，若其中有 m 个针压到了某条线，则 $\pi$ 的近似值为 $\frac{2M}{m}$。

## 解析

布丰投针问题的数学原理：

1. 设针长为 $\ell$，平行线间距为 $d$
2. 针与任意一条线相交的概率为 $P(\text{相交})=\frac{2\ell}{\pi d}$
3. 当 $\ell = d$ 时，$\pi = \frac{2N_{\text{总}}}{N_{\text{相交}}}$，其中：
   - $N_{\text{总}}$ 是投掷总次数
   - $N_{\text{相交}}$ 是相交次数

判断相交的条件：针中点到最近平行线的距离 $y \leq \frac{\ell}{2}|\sin(\theta)|$，其中 $\theta$ 是针与线的夹角。

### 证明

设针中点到最近平行线的距离为 $y$，针与线的夹角为 $\theta$，则：

1. $y$ 在 $[0,\frac{d}{2}]$ 上均匀分布
2. $\theta$ 在 $[0,\pi]$ 上均匀分布
3. 相交概率为：

$$P(\text{相交})=\frac{2}{\pi}\int_0^{\pi/2}\frac{\ell\sin\theta}{d}d\theta=\frac{2\ell}{\pi d}$$

## 答案

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

int main(void) {
    int M;
    printf("输入投掷次数：");
    scanf("%d", &M);
    
    srand(time(NULL));
    int cross = 0;
    const double d = 1.0;  // 线间距
    const double l = 1.0;  // 针长
    
    for(int i = 0; i < M; i++) {
        // y是针中点到最近线的距离
        double y = (double)rand() / RAND_MAX * (d/2);
        // theta是针与线的夹角
        double theta = (double)rand() / RAND_MAX * M_PI;
        
        // 判断是否相交
        if(y <= (l/2) * sin(theta)) {
            cross++;
        }
    }
    
    double pi = 2.0 * M / cross;
    printf("π ≈ %.6f\n", pi);
    return 0;
}
```
