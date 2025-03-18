/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#ifndef MDR_SEQLIST_H
#define MDR_SEQLIST_H

#include <stdio.h>
#include <stdlib.h>

constexpr int g_initCap = 4;

typedef int ElementType;

typedef struct {
    ElementType *arr;
    size_t size;     // length of the sequence list
    size_t capacity; // size of the array
} SeqList;

void initSeqList(SeqList *l) {
    l->arr = (ElementType *)malloc(sizeof(ElementType) * g_initCap);
    if (!(l->arr)) {
        printf("Initialization failed!\n");
        exit(-1);
    }
    l->size = 0;
    l->capacity = g_initCap;
}

void initSeqListFromArray(SeqList *l, const ElementType *a, int length) {
    l->arr = (ElementType *)malloc(sizeof(ElementType) * length);
    if (!(l->arr)) {
        printf("Initialization failed!\n");
        exit(-1);
    }
    for (size_t n = 0; n < length; ++n) {
        l->arr[n] = a[n];
    }
    l->size = length;
    l->capacity = length;
}

void clearSeqList(SeqList *l) {
    free(l->arr);
    initSeqList(l);
}

bool isSeqListEmpty(const SeqList *l) { return l->size == 0; }

size_t lengthOfSeqList(const SeqList *l) { return l->size; }

ElementType getFromSeqList(const SeqList *l, int i) {
    if (i >= l->size) {
        printf("Index out of bound!\n");
        return 0;
    }
    return l->arr[i];
}

void expandSeqList(SeqList *l) {
    printf("Expansion\n");
    ElementType *newArr =
        (ElementType *)realloc(l->arr, sizeof(ElementType) * 2 * l->capacity);
    if (!(newArr)) {
        printf("Expansion failed!\n");
        exit(-1);
    }
    l->capacity *= 2;
    l->arr = newArr;
}

void curtailSeqList(SeqList *l) {
    printf("Curtailment\n");
    ElementType *newArr =
        (ElementType *)realloc(l->arr, sizeof(ElementType) * l->capacity / 2);
    if (!newArr) {
        printf("Curtailment failed!\n");
        exit(-1);
    }
    l->capacity /= 2;
    l->arr = newArr;
}

void insertBack2SeqList(SeqList *l, ElementType x) {
    if (l->size == l->capacity) {
        expandSeqList(l);
    }
    l->arr[l->size] = x;
    l->size += 1;
}

void insert2SeqList(SeqList *l, int i, ElementType x) {
    if (l->size == l->capacity) {
        expandSeqList(l);
    }
    for (size_t n = l->size; n > i; --n) {
        l->arr[n] = l->arr[n - 1];
    }
    l->arr[i] = x;
    l->size += 1;
}

void removeFromSeqList(SeqList *l, int i) {
    if (i >= l->size)
        printf("Index out of bound!\n");
    for (size_t n = i; n < l->size - 1; ++n) {
        l->arr[n] = l->arr[n + 1];
    }
    l->size -= 1;
    if ((l->size + 1) * 4 < l->capacity) {
        curtailSeqList(l);
    }
}

int indexOfSeqList(const SeqList *l, ElementType x) {
    for (int n = 0; n < l->size; ++n) {
        if (l->arr[n] == x) {
            return n;
        }
    }
    return -1;
}

void printSeqList(const SeqList *l) {
    printf("cap: %zu  size: %zu ==>", l->capacity, l->size);
    for (size_t i = 0; i < l->size; ++i) {
        printf(" [%zu]=%d", i, l->arr[i]);
    }
    printf("\n");
}

#endif
