#include <stdio.h>

// 函数声明
double add(double a, double b);
double subtract(double a, double b);
double multiply(double a, double b);
double divide(double a, double b);

int main()
{
    char operator;
    double num1, num2, result;

    // 获取用户输入
    printf("请输入两个数字: ");
    scanf("%lf %lf", &num1, &num2);
    printf("请输入运算符(+, -, *, /): ");
    scanf("%c", &operator);

    // 判断运算符是否有效
    // 根据运算符进行计算
    switch (operator) {
    case '+':
        result = add(num1, num2);
        break;
    case '-':
        result = subtract(num1, num2);
        break;
    case '*':
        result = multiply(num1, num2);
        break;
    case '/':
        result = divide(num1, num2);
        if (num2 == 0) {
            return 22;
        }
        break;
    default:
        perror("无效的运算符");
        return 22;
    }

    printf("结果: %lf\n", result);

    return 0;
}

// 加法函数
double add(double a, double b)
{
    return a + b;
}

// 减法函数
double subtract(double a, double b)
{
    return a - b;
}

// 乘法函数
double multiply(double a, double b)
{
    return a * b;
}

// 除法函数
double divide(double a, double b)
{
    if (b == 0) {
        perror("除数不能为0\n");
        return 0.0;
    }
    return a / b;
}
