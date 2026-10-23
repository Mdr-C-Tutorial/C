CC     ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -g

SRCS := $(wildcard *.c)
BINS := $(patsubst %.c,%,$(SRCS))

.PHONY: all clean

all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) $(BINS)
