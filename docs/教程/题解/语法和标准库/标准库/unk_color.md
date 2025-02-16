# RGB HSV

## 题目

实现 RGB 与 HSV 颜色模型之间的转换。

## 解析

## 答案

```c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RGB;

typedef struct {
    double h; // 色相 (0-360)
    double s; // 饱和度 (0-1)
    double v; // 明度 (0-1)
} HSV;

HSV rgb_to_hsv(RGB rgb) {
    HSV hsv;

    double r = rgb.r / 255.0;
    double g = rgb.g / 255.0;
    double b = rgb.b / 255.0;

    double max_val = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    double min_val = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    double chroma = max_val - min_val;

    if (chroma == 0) {
        hsv.h = 0; // 无彩色
    } else if (max_val == r) {
        hsv.h = 60.0 * fmod(((g - b) / chroma), 6.0);
        if (hsv.h < 0) {
          hsv.h += 360.0;
        }
    } else if (max_val == g) {
        hsv.h = 60.0 * (((b - r) / chroma) + 2.0);
    } else { // max_val == b
        hsv.h = 60.0 * (((r - g) / chroma) + 4.0);
    }

    if (max_val == 0) {
        hsv.s = 0;
    } else {
        hsv.s = chroma / max_val;
    }

    hsv.v = max_val;

    return hsv;
}

RGB hsv_to_rgb(HSV hsv) {
    RGB rgb;

    double h = hsv.h;
    double s = hsv.s;
    double v = hsv.v;

    double chroma = v * s;
    double x = chroma * (1 - fabs(fmod(h / 60.0, 2.0) - 1));
    double m = v - chroma;

    double r_prime, g_prime, b_prime;

    if (h >= 0 && h < 60) {
        r_prime = chroma;
        g_prime = x;
        b_prime = 0;
    } else if (h >= 60 && h < 120) {
        r_prime = x;
        g_prime = chroma;
        b_prime = 0;
    } else if (h >= 120 && h < 180) {
        r_prime = 0;
        g_prime = chroma;
        b_prime = x;
    } else if (h >= 180 && h < 240) {
        r_prime = 0;
        g_prime = x;
        b_prime = chroma;
    } else if (h >= 240 && h < 300) {
        r_prime = x;
        g_prime = 0;
        b_prime = chroma;
    } else { // h >= 300 && h < 360
        r_prime = chroma;
        g_prime = 0;
        b_prime = x;
    }

    rgb.r = (unsigned char)((r_prime + m) * 255);
    rgb.g = (unsigned char)((g_prime + m) * 255);
    rgb.b = (unsigned char)((b_prime + m) * 255);

    return rgb;
}

// 打印 RGB 值
void print_rgb(RGB rgb) {
    printf("RGB: (%d, %d, %d)\n", rgb.r, rgb.g, rgb.b);
}

// 打印 HSV 值 (使用 double)
void print_hsv(HSV hsv) {
    printf("HSV: (%.2f, %.2f, %.2f)\n", hsv.h, hsv.s, hsv.v);
}


int main() {
    // 示例 1：RGB 转 HSV
    RGB rgb1 = {255, 0, 0}; // 红色
    HSV hsv1 = rgb_to_hsv(rgb1);
    printf("Example 1:\n");
    print_rgb(rgb1);
    print_hsv(hsv1);

    // 示例 2：HSV 转 RGB
    HSV hsv2 = {120, 1, 1}; // 纯绿色
    RGB rgb2 = hsv_to_rgb(hsv2);
    printf("\nExample 2:\n");
    print_hsv(hsv2);
    print_rgb(rgb2);

    // 示例 3：RGB 转 HSV (另一种颜色)
    RGB rgb3 = {100, 150, 200};
    HSV hsv3 = rgb_to_hsv(rgb3);
    printf("\nExample 3:\n");
    print_rgb(rgb3);
    print_hsv(hsv3);

    // 示例 4：HSV 转 RGB (另一种颜色)
    HSV hsv4 = {200, 0.7, 0.8};
    RGB rgb4 = hsv_to_rgb(hsv4);
    printf("\nExample 4:\n");
    print_hsv(hsv4);
    print_rgb(rgb4);

    // 边界条件测试... (和之前一样，省略以节省空间)
    // 示例 5 测试边界条件：黑色
    RGB rgb5 = {0, 0, 0};
    HSV hsv5 = rgb_to_hsv(rgb5);
    printf("\nExample 5 (Black):\n");
    print_rgb(rgb5);
    print_hsv(hsv5);

    // 示例 6 测试边界条件：白色
    RGB rgb6 = {255, 255, 255};
    HSV hsv6 = rgb_to_hsv(rgb6);
    printf("\nExample 6 (White):\n");
    print_rgb(rgb6);
    print_hsv(hsv6);

    // 示例 7 测试边界条件：灰色
    RGB rgb7 = {128, 128, 128};
    HSV hsv7 = rgb_to_hsv(rgb7);
    printf("\nExample 7 (Gray):\n");
    print_rgb(rgb7);
    print_hsv(hsv7);

    // 示例8  测试非整数hsv
    HSV hsv8 = {200.5, 0.75, 0.85};
    RGB rgb8 = hsv_to_rgb(hsv8);
    printf("\nExample 8 (Non-integer HSV):\n");
    print_hsv(hsv8);
    print_rgb(rgb8);
    return 0;
}
```
