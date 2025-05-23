/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#ifndef MDR_LINKEDLIST_H
#define MDR_LINKEDLIST_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef int ElementType;

typedef struct Node {
    ElementType n;
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    size_t length;
} LinkedList;

void initLinkedList(LinkedList *l) {
    l->head = NULL;
    l->length = 0;
}

void initLinkedListWithValue(LinkedList *l, ElementType x) {
    l->head = (Node *)malloc(sizeof(Node));
    if (l->head == NULL) {
        printf("initialization failed!");
        exit(-1);
    }
    l->head->n = x;
    l->head->next = NULL;
    l->length = 1;
}

bool isLinkedListEmpty(const LinkedList *l) { return l->length == 0; }

size_t lengthOfLinkedList(const LinkedList *l) { return l->length; }

void insert2LinkedList(LinkedList *l, ElementType x) {
    Node *node = l->head;
    l->head = (Node *)malloc(sizeof(Node));
    l->head->n = x;
    l->head->next = node;
    l->length += 1;
}

int indexOfLinkedList(const LinkedList *l, const ElementType x) {
    int index = 0;
    Node *node = l->head;
    if (node == NULL)
        return -1;
    do {
        if (node->n == x)
            return index;
        index += 1;
    } while ((node = node->next) != NULL);
    return -1;
}

Node *nodeInLinkedList(const LinkedList *l, ElementType x) {
    Node *node = l->head;
    if (node == NULL)
        return NULL;
    do {
        if (node->n == x) {
            return node;
        }
    } while ((node = node->next) != NULL);
    return NULL;
}

int deleteNode(LinkedList *l, ElementType x) {
    int cnt = 0;
    Node *first = l->head;
    while (first != NULL && first->n == x) {
        Node *tmp = first;
        first = first->next;
        free(tmp);
        cnt += 1;
    }
    l->head = first;
    if (first) {
        Node *node = first->next;
        while (node != NULL) {
            if (node->n == x) {
                Node *tmp = node;
                node = node->next;
                free(tmp);
                cnt += 1;
            } else {
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

void deleteFirstByValue(LinkedList *l, ElementType x) {
    Node *node = l->head;
    if (node == NULL)
        return;
    if (node->n == x) {
        l->head = node->next;
        free(node);
        l->length -= 1;
        return;
    }
    Node *prev = NULL;
    while (node != NULL && node->n != x) {
        // 至少执行 1 次
        prev = node;
        node = node->next;
    }
    if (node != NULL) {
        prev->next = node->next;
        free(node);
    }
}

void deleteFirst(LinkedList *l) {
    Node *tmp = l->head;
    l->head = l->head->next;
    free(tmp);
    l->length -= 1;
}

void clearLinkedList(LinkedList *l) {
    while (lengthOfLinkedList(l) > 0) {
        deleteFirst(l);
    }
}

LinkedList merge(const LinkedList *a, const LinkedList *b) {
    LinkedList c;
    c.head = a->head;
    Node *node = c.head;
    while (node->next) {
        node = node->next;
    }
    node->next = b->head;
    c.length = a->length + b->length;
    return c;
}

void printLinkedList(const LinkedList *l) {
    printf("%zu: ", l->length);
    int index = 0;
    Node *node = l->head;
    if (node == NULL) {
        printf("\n");
        return;
    }
    do {
        printf("[%d]=%d ", index, node->n);
        index += 1;
    } while ((node = node->next) != NULL);
    printf("\n");
}
