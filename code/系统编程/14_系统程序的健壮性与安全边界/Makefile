CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -g

PROGRAMS = rlim emfile linegrow mycp cmd_inject spawn_safe toctou toctou_fixed \
           jail nnp strict strict_exit inherit fdlist fdleak cr drop_priv \
           asan_demo secenv tmpsafe ckd lecho_hardened

all: $(PROGRAMS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

asan_demo: asan_demo.c
	$(CC) $(CFLAGS) -fsanitize=address,undefined $< -o $@

ckd: ckd.c
	$(CC) -std=c2x -Wall -Wextra -g $< -o $@

clean:
	rm -f $(PROGRAMS) *.o

.PHONY: all clean