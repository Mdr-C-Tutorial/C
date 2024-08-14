#include <stdio.h>
// #define fastin
int main()
{
    char ch, op;
    long long a = 0, b = 0;
#ifdef fastin
    for (ch = getchar(); ch < '0' || '9' < ch; ch = getchar())
        ;
    for (; '0' <= ch && ch <= '9'; ch = getchar())
        a = a * 10 + ch - '0';
    for (op = ch; op != '+' && op != '-' && op != '*' && op != '/'; op = getchar())
        ;
    for (ch = getchar(); ch < '0' || '9' < ch; ch = getchar())
        ;
    for (; '0' <= ch && ch <= '9'; ch = getchar())
        b = b * 10 + ch - '0';
#else
    scanf("%llu%c%llu", &a, &op, &b);
#endif
    switch (op)
    {
    case '+':
        printf("%lld+%lld=%lld", a, b, a + b);
        break;
    case '-':
        printf("%lld-%lld=%lld", a, b, a - b);
        break;
    case '*':
        printf("%lld*%lld=%lld", a, b, a * b);
        break;
    case '/':
        if (b)
            printf("%lld/%lld=%lld......%lld", a, b, a / b, a % b);
        else
            perror("Cannot be divided by 0!");
        break;
    default:
        perror("Wrong operation!");
        break;
    }
    return 0;
}
