/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#ifndef MDR_STACK_ARRAY_STATIC_H
#define MDR_STACK_ARRAY_STATIC_H
#include <stdio.h>

constexpr int cap = 100;
typedef int ElementType;

typedef struct
{
    int array[100];
    int top;
} Stack;

void initStack(Stack* s)
{
    s->top = -1;
    for (int i = 0; i < cap; i++) {
        s->array[i] = 0;
    }
}

void push(Stack* s, ElementType e)
{
    if (s->top == cap - 1) {
        printf("Stack Overflow\n");
    } else {
        s->array[++s->top] = e;
    }
}

ElementType pop(Stack* s)
{
    if (s->top == -1) {
        printf("Stack Underflow\n");
        return 0;
    }
    return s->array[s->top--];
}

void printStack(const Stack* s)
{
    printf("%d element", s->top + 1);
    if (s->top == -1 || s->top == 0) {
        printf(": [ ");
    } else {
        printf("s: [ ");
    }
    for (int i = 0; i <= s->top; i++) {
        printf("%d ", s->array[i]);
    }
    printf("]\n");
}

#endif // MDR_STACK_ARRAY_STATIC_H
