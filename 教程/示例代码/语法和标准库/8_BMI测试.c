#include <assert.h>
#include <stdio.h>

int main()
{
    float weight, height, bmi;

    // 获取用户输入
    printf("请输入您的体重（单位：千克）：");
    scanf("%f", &weight);

    assert(weight != 0);

    printf("请输入您的身高（单位：厘米）：");
    scanf("%f", &height);

    assert(height != 0);

    // 换算
    height /= 100;

    // 计算BMI
    bmi = weight / (height * height);

    // 显示结果
    printf("您的BMI值为: %.2f\n", bmi);

    return 0;
}