CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all run clean

all: pong

pong: pong.c
	$(CC) $(CPPFLAGS) $(CFLAGS) pong.c $(LDFLAGS) $(LDLIBS) -o $@

run: pong
	./pong

clean:
	rm -f pong
