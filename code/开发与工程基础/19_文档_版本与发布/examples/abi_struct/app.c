#include "wf/counter.h"

#include <stdio.h>

int main(void) {
    wf_counter *counter = wf_counter_create();
    wf_entry entries[3];
    size_t n = wf_counter_top(counter, entries, 3);
    for (size_t i = 0; i < n; i++) {
        printf("%7lu %s\n", (unsigned long)entries[i].count, entries[i].word);
    }
    wf_counter_destroy(counter);
    return 0;
}
