CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O2

all: resp_client

resp_client: resp_client.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f resp_client

.PHONY: all clean