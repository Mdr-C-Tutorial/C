#include "ast.h"

#include <stdlib.h>
#include <string.h>

struct arena_chunk {
    arena_chunk *next;
    size_t used;
    size_t cap;
    unsigned char data[];
};

#define ARENA_CHUNK_SIZE 4096u

void arena_init(arena *a) {
    a->head = NULL;
}

void *arena_alloc(arena *a, size_t size) {
    size = (size + 7u) & ~(size_t)7u;
    if (a->head == NULL || a->head->cap - a->head->used < size) {
        size_t cap = size > ARENA_CHUNK_SIZE ? size : ARENA_CHUNK_SIZE;
        arena_chunk *chunk = (arena_chunk *)malloc(sizeof(*chunk) + cap);
        if (chunk == NULL) {
            return NULL;
        }
        chunk->next = a->head;
        chunk->used = 0u;
        chunk->cap = cap;
        a->head = chunk;
    }
    void *mem = a->head->data + a->head->used;
    a->head->used += size;
    return mem;
}

void arena_free(arena *a) {
    arena_chunk *chunk = a->head;
    while (chunk != NULL) {
        arena_chunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    a->head = NULL;
}

node *node_new(arena *a, node_kind kind) {
    node *n = (node *)arena_alloc(a, sizeof(*n));
    if (n == NULL) {
        return NULL;
    }
    memset(n, 0, sizeof(*n));
    n->kind = kind;
    return n;
}

bool node_list_push(arena *a, node_list *list, node *item) {
    if (list->len == list->cap) {
        size_t next = list->cap == 0u ? 8u : list->cap * 2u;
        node **items = (node **)arena_alloc(a, next * sizeof(*items));
        if (items == NULL) {
            return false;
        }
        if (list->items != NULL) {
            memcpy(items, list->items, list->len * sizeof(*items));
        }
        list->items = items;
        list->cap = next;
    }
    list->items[list->len++] = item;
    return true;
}