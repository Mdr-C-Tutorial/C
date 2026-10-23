CC     ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -g
LDLIBS := -pthread

PROGS := count_c11 count_pthread arg_bug arg_ok create_join tids stacksize affinity \
         schedinfo ctxsw errorcheck futex_count race cond_clock bqueue rwlock \
         barrier sem_once robust shared fork_threads sig_handler_threads lock_only \
         sigwait_thread tpool_demo pexit detach_join pthread_error

.PHONY: all clean tsan

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

# tpool_demo 需要 tpool.o
tpool_demo: tpool_demo.c tpool.c tpool.h
	$(CC) $(CFLAGS) tpool_demo.c tpool.c -o $@ $(LDLIBS)

# 线程竞争检测：用 race.c 编一份带 ThreadSanitizer 的版本
tsan: race_tsan

race_tsan: race.c
	$(CC) $(CFLAGS) -fsanitize=thread race.c -o $@ $(LDLIBS)

clean:
	$(RM) $(PROGS) race_tsan tpool.o