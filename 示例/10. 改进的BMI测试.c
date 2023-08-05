#include <stdio.h>

int main()
{
    float weight, height, bmi;

    // 获取用户输入
    printf("请输入您的体重（单位：千克）：");
    scanf("%f", &weight);
    printf("请输入您的身高（单位：厘米）：");
    scanf("%f", &height);

    // 换算
    height /= 100;

    // 计算BMI
    bmi = weight / (height * height);

    // 显示结果
    printf("您的BMI值为: %.2f\n", bmi);

    // 判断健康状态
    if (bmi < 18.5)
    {
        printf("您的体重过轻。\n");
    }
    else if (bmi >= 18.5 && bmi < 24.9)
    {
        printf("您的体重正常。\n");
    }
    else if (bmi >= 25 && bmi < 29.9)
    {
        printf("您的体重过重。\n");
    }
    else
    {
        printf("您的体重肥胖。\n");
    }

    return 0;
}