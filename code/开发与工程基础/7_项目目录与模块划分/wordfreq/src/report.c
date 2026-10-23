#include "wf/report.h"

void wf_report_print(FILE *out, const wf_entry *entries, size_t n) {
    for (size_t i = 0; i < n; i++) {
        fprintf(out, "%7zu %s\n", entries[i].count, entries[i].word);
    }
}
