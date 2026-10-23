#ifndef WF_COUNTER_H
#define WF_COUNTER_H

#include <stddef.h>

typedef struct wf_counter wf_counter;

typedef struct {
    const char *word;
    size_t count;
    size_t rank;
} wf_entry;

wf_counter *wf_counter_create(void);
void wf_counter_destroy(wf_counter *c);
size_t wf_counter_top(const wf_counter *c, wf_entry *out, size_t n);

#endif
