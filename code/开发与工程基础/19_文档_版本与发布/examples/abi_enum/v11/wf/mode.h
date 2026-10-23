#ifndef WF_MODE_H
#define WF_MODE_H

/* 1.1.0：在开头插入一项，后面两项的值都变了 */
enum wf_mode { WF_MODE_COMPACT = 0, WF_MODE_FAST = 1, WF_MODE_EXACT = 2 };

int wf_passes(enum wf_mode mode);

#endif
