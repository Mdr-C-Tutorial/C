#include <stdint.h>
#include <stdlib.h>

#define VECTOR_OBJECT(T) \
    typedef struct {     \
        size_t l;        \
        size_t cmaxl;    \
        T* data;         \
    } vector_##T, *vector_ref_##T;

#define VECTOR_PUSH_BACK(T)                          \
    void vector_push_back_##T(vector_ref_##T v, T d) \
    {                                                \
        if (v->l < v->cmaxl) {                       \
            v->data[v->l] = d;                       \
            (v->l)++;                                \
        } else {                                     \
            size_t n = v->cmaxl * 2;                 \
            T* t = malloc(n * sizeof(T));            \
            for (size_t i = 0; i < v->l; i++) {      \
                t[i] = v->data[i];                   \
            }                                        \
            free(v->data);                           \
            v->data = t;                             \
            v->cmaxl = n;                            \
            v->data[v->l] = d;                       \
            (v->l)++;                                \
        }                                            \
    }

#define VECTOR_POP_BACK(T)                     \
    bool vector_pop_back_##T(vector_ref_##T v) \
    {                                          \
        if (v->l > 0) {                        \
            (v->l)--;                          \
            return true;                       \
        }                                      \
        return false;                          \
    }

#define VECTOR_BACK(T)                   \
    T* vector_back_##T(vector_ref_##T v) \
    {                                    \
        if (v->l == 0) {                 \
            return NULL;                 \
        } else {                         \
            return &(v->data[v->l - 1]); \
        }                                \
    }

#define VECTOR_FREE_UNUSED_MEMORY(T)                     \
    void vector_free_unused_memory_##T(vector_ref_##T v) \
    {                                                    \
        T* t = malloc(v->l * sizeof(T));                 \
        for (size_t i = 0; i < v->l; i++) {              \
            t[i] = v->data[i];                           \
        }                                                \
        free(v->data);                                   \
        v->data = t;                                     \
        v->cmaxl = v->l;                                 \
    }

#define VECTOR_PUSH_FRONT(T)                          \
    void vector_push_front_##T(vector_ref_##T v, T f) \
    {                                                 \
        if (v->cmaxl > v->l) {                        \
            T* e = &(v->data[v->l - 1]);              \
            while (e != (v->data) - 1) {              \
                *(e + 1) = *e;                        \
                e--;                                  \
            }                                         \
            v->data[0] = f;                           \
            (v->l)++;                                 \
        } else {                                      \
            size_t n = v->cmaxl * 2;                  \
            T* t = malloc(n * sizeof(T));             \
            t[0] = f;                                 \
            for (size_t i = 0; i < v->l; i++) {       \
                t[i + 1] = v->data[i];                \
            }                                         \
            free(v->data);                            \
            v->data = t;                              \
            (v->l)++;                                 \
            v->cmaxl = n;                             \
        }                                             \
    }

#define VECTOR_POP_FRONT(T)                         \
    bool vector_pop_front_##T(vector_ref_##T v)     \
    {                                               \
        if (v->l > 0) {                             \
            for (size_t i = 0; i < v->l - 1; i++) { \
                v->data[i] = v->data[i + 1];        \
            }                                       \
            (v->l)--;                               \
            return true;                            \
        } else {                                    \
            return false;                           \
        }                                           \
    }

#define VECTOR_FRONT(T)                   \
    T* vector_front_##T(vector_ref_##T v) \
    {                                     \
        if (v->l == 0) {                  \
            return NULL;                  \
        } else {                          \
            return &(v->data[0]);         \
        }                                 \
    }

#define VECTOR_ALLOC(T)                                    \
    vector_ref_##T vector_alloc_##T()                      \
    {                                                      \
        vector_ref_##T v = malloc(sizeof(vector_ref_##T)); \
        v->cmaxl = 10;                                     \
        v->l = 10;                                         \
        v->data = malloc(sizeof(T) * 10);                  \
        return v;                                          \
    }

#define VECTOR_FREE(T)                     \
    void vector_free_##T(vector_ref_##T v) \
    {                                      \
        free(v->data);                     \
        free(v);                           \
    }

#define VECTOR(T)                \
    VECTOR_OBJECT(T)             \
    VECTOR_PUSH_BACK(T)          \
    VECTOR_POP_BACK(T)           \
    VECTOR_BACK(T)               \
    VECTOR_PUSH_FRONT(T)         \
    VECTOR_POP_FRONT(T)          \
    VECTOR_FRONT(T)              \
    VECTOR_FREE_UNUSED_MEMORY(T) \
    VECTOR_ALLOC(T)              \
    VECTOR_FREE(T)

VECTOR(int)

int main()
{
}
