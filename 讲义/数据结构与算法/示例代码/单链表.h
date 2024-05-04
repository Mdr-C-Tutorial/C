#ifndef MDR_LINKEDLIST_H
#define MDR_LINKEDLIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef int ElementType;

typedef struct Node
{
    ElementType n;
    struct Node *next;
} Node;

typedef struct
{
    Node *head;
    size_t length;
} LinkedList;

void initLinkedList(LinkedList *l)
{
    l->head = NULL;
    l->length = 0;
}

void initLinkedListWithValue(LinkedList *l, ElementType x)
{
    l->head = (Node *)malloc(sizeof(Node));
    if (l->head == NULL)
    {
        printf("initialization failed!");
        exit(-1);
    }
    l->head->n = x;
    l->head->next = NULL;
    l->length = 1;
}

bool isLinkedListEmpty(LinkedList *l) { return l->length == 0; }

size_t lengthOfLinkedList(LinkedList *l) { return l->length; }

void insert2LinkedList(LinkedList *l, ElementType x)
{
    Node *node = l->head;
    l->head = (Node *)malloc(sizeof(Node));
    l->head->n = x;
    l->head->next = node;
    l->length += 1;
}

int indexOfLinkedList(LinkedList *l, ElementType x)
{
    int index = 0;
    Node *node = l->head;
    if (node == NULL)
        return -1;
    do
    {
        if (node->n == x)
            return index;
        index += 1;
    } while ((node = node->next) != NULL);
    return -1;
}

Node *nodeInLinkedList(LinkedList *l, ElementType x)
{
    Node *node = l->head;
    if (node == NULL)
        return NULL;
    do
    {
        if (node->n == x)
            return node;
    } while ((node = node->next) != NULL);
    return NULL;
}
#if 0
int deleteNode(LinkedList* l, ElementType x) {
    Node* node = l->head;
    if (node == NULL) // 链表长度为 0
        return 0;
    if (l->head->n == x) {
        Node* node = l->head;
        l->head = node->next;
        free(node);
        l->length -= 1;
        return deleteNode(l, x) + 1;
    }
    if (node->next == NULL) { // 链表长度为 1
        if (node->n == x) {
            l->head = NULL;
            free(node);
            l->length -= 1;
            return 1;
        } else {
            return 0;
        }
    }

    // 运行到此处，已经确定链表长度 >= 2
    Node* prev = node;
    node = node->next; // 此时prev指向l->head，node指向第二个节点
    int cnt = 0;
    do {
        if (node->n == x) {
            prev->next = node->next;
            free(node);
            cnt += 1;
            node = prev->next;
        } else {
            prev = node;
            node = node->next;
        }
    } while (node != NULL);
    l->length -= cnt;
    return cnt;
}
#else

int deleteNode(LinkedList *l, ElementType x)
{
    int cnt = 0;
    Node *first = l->head;
    while (first != NULL && first->n == x)
    {
        Node *tmp = first;
        first = first->next;
        free(tmp);
        cnt += 1;
    }
    l->head = first;
    if (first)
    {
        Node *node = first->next;
        while (node != NULL)
        {
            if (node->n == x)
            {
                Node *tmp = node;
                node = node->next;
                free(tmp);
                cnt += 1;
            }
            else
            {
                first->next = node;
                first = node;
                node = node->next;
            }
        }
        first->next = NULL;
    }
    l->length -= cnt;
    return cnt;
}

#endif

void deleteFirstByValue(LinkedList *l, ElementType x)
{
    Node *node = l->head;
    if (node == NULL)
        return;
    if (node->n == x)
    {
        l->head = node->next;
        free(node);
        l->length -= 1;
        return;
    }
    Node *prev = NULL;
    while (node != NULL && node->n != x)
    { // 至少执行 1 次
        prev = node;
        node = node->next;
    }
    if (node != NULL)
    {
        prev->next = node->next;
        free(node);
    }
}

void deleteFirst(LinkedList *l)
{
    Node *tmp = l->head;
    l->head = l->head->next;
    free(tmp);
    l->length -= 1;
}

void clearLinkedList(LinkedList *l)
{
    while (lengthOfLinkedList(l) > 0)
    {
        deleteFirst(l);
    }
}

LinkedList merge(LinkedList *a, LinkedList *b)
{
    LinkedList c;
    c.head = a->head;
    Node *node = c.head;
    while (node->next)
    {
        node = node->next;
    }
    node->next = b->head;
    c.length = a->length + b->length;
    return c;
}

void printLinkedList(LinkedList *l)
{
    printf("%zu: ", l->length);
    int index = 0;
    Node *node = l->head;
    if (node == NULL)
    {
        printf("\n");
        return;
    }
    do
    {
        printf("[%d]=%d ", index, node->n);
        index += 1;
    } while ((node = node->next) != NULL);
    printf("\n");
}

#endif