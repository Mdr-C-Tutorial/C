#ifndef WF_MODE_H
#define WF_MODE_H

/* 1.0.0：两项 */
enum wf_mode { WF_MODE_FAST = 0, WF_MODE_EXACT = 1 };

int wf_passes(enum wf_mode mode);

#endif
