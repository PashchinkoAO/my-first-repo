CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wpedantic

.PHONY: all run clean

all: pong

pong: src/pong.c
	$(CC) $(CPPFLAGS) $(CFLAGS) src/pong.c $(LDFLAGS) $(LDLIBS) -o $@

run: pong
	./pong

clean:
	rm -f pong
