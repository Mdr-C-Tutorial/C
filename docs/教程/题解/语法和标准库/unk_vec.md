# unk_vec

## 题目

输入维数 n，再输入两个 n 维向量在某直角坐标系内的坐标，输出它们的夹角。

## 解析

$$\cos\theta=\frac{\vec{a}\cdot\vec{b}}{|\vec{a}|\cdot|\vec{b}|}$$

## 答案

```c
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// 判断是否为零向量
int is_zero_vector(const double* v, int n) {
    for (int i = 0; i < n; i++) {
        if (fabs(v[i]) > 1e-10) return 0;  // 使用小阈值判断是否为 0
    }
    return 1;
}

double dot_product(const double* a, const double* b, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

double vector_length(const double* v, int n) {
    return sqrt(dot_product(v, v, n));
}

int main(void) {
    int n;
    printf("请输入向量维数：");
    scanf("%d", &n);

    if (n <= 0) {
        printf("错误：维数必须为正整数\n");
        return 1;
    }

    double* v1 = malloc(n * sizeof(double));
    double* v2 = malloc(n * sizeof(double));

    if (!v1 || !v2) {
        printf("错误：内存分配失败\n");
        free(v1);
        free(v2);
        return 1;
    }

    printf("请输入第一个向量的坐标：\n");
    for (int i = 0; i < n; i++) {
        scanf("%lf", &v1[i]);
    }

    printf("请输入第二个向量的坐标：\n");
    for (int i = 0; i < n; i++) {
        scanf("%lf", &v2[i]);
    }

    // 检查零向量
    if (is_zero_vector(v1, n) || is_zero_vector(v2, n)) {
        printf("错误：零向量没有方向，无法计算夹角\n");
        free(v1);
        free(v2);
        return 1;
    }

    double cos_theta = dot_product(v1, v2, n) /
                      (vector_length(v1, n) * vector_length(v2, n));

    // 处理数值误差导致的 cos_theta > 1 或 cos_theta < -1 的情况
    if (cos_theta > 1) cos_theta = 1;
    if (cos_theta < -1) cos_theta = -1;

    double angle = acos(cos_theta) * 180.0 / M_PI;

    printf("两向量的夹角为：%.2f 度\n", angle);

    free(v1);
    free(v2);
    return 0;
}
```

## 题外话

在高维空间中，随机选择两个向量时，这两个向量垂直或接近垂直的概率会随着维数的增加而增大。

### 数学证明

考虑在 n 维空间中随机选择两个单位向量 $\vec{a}$ 和 $\vec{b}$。它们的夹角 θ 满足：

$$\cos\theta=\vec{a}\cdot\vec{b}=\sum_{i=1}^n a_i b_i$$

根据大数定律，当 n 趋于无穷时，这个点积会趋近于 0（因为每一项 $a_i b_i$ 的期望为 0）。具体地：

1. 设 $X_i = a_i b_i$，则 $\mathbb{E}(X_i) = 0$（因为 $a_i$ 和 $b_i$ 独立且对称分布）

2. $\mathrm{Var}(X_i) = \frac{1}{n^2}$（因为是单位向量）

3. 根据中心极限定理，当 $n \to \infty$ 时：

   $$\frac{\sum_{i=1}^n X_i}{\sqrt{\mathrm{Var}(\sum_{i=1}^n X_i)}} \sim \mathcal{N}(0,1)$$

4. 因此：

   $$\mathrm{P}(|\cos\theta| < \epsilon) \to 1 \text{ 当 } n \to \infty$$

这说明在高维空间中，随机选择的两个向量很可能接近垂直（夹角接近 90°）。

### 直观理解

可以这样理解：在高维空间中，向量的分量越多，它们的点积中的正项和负项相互抵消的机会就越大，使得最终的点积趋近于零，即夹角趋近于 90°。
