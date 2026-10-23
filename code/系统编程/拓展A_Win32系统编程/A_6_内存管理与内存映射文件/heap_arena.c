/* 私有堆：分配 1000 个节点，最后一次 HeapDestroy 全部归还 */
#include <windows.h>

#include <stdio.h>

#define NODE_COUNT 1000

struct node {
    struct node *next;
    long value;
    char label[24];
};

int main(void) {
    HANDLE heap = HeapCreate(0, 64 * 1024, 0);
    if (heap == NULL) {
        printf("HeapCreate failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    printf("private heap created, handle %p\n", (void *)heap);

    struct node *head = NULL;
    for (int i = 0; i < NODE_COUNT; i++) {
        struct node *n = HeapAlloc(heap, 0, sizeof *n);
        if (n == NULL) {
            printf("HeapAlloc failed after %d nodes: %lu\n", i,
                   (unsigned long)GetLastError());
            HeapDestroy(heap);
            return 1;
        }
        n->value = i;
        snprintf(n->label, sizeof n->label, "node-%03d", i);
        n->next = head;
        head = n;
    }

    long sum = 0;
    int count = 0;
    for (struct node *n = head; n != NULL; n = n->next) {
        sum += n->value;
        count++;
    }
    printf("allocated %d nodes, sum of values = %ld\n", count, sum);
    printf("sizeof(struct node) = %llu, HeapSize of the head = %llu\n",
           (unsigned long long)sizeof(struct node),
           (unsigned long long)HeapSize(heap, 0, head));

    /* 同一个进程的默认堆是另一块地方，两个堆互不相干 */
    HANDLE def = GetProcessHeap();
    void *p = HeapAlloc(def, 0, 1000);
    printf("default heap handle %p, 1000 bytes at %p, HeapSize %llu\n",
           (void *)def, p, (unsigned long long)HeapSize(def, 0, p));
    HeapFree(def, 0, p);

    if (!HeapDestroy(heap)) {
        printf("HeapDestroy failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    printf("HeapDestroy returned, all %d nodes are gone at once\n", count);
    return 0;
}