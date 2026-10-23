CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wpedantic
NCURSES_LIBS ?= -lncurses
PYTHON ?= python3

.PHONY: all run clean life run-life test-life check-life

all: pong

pong: src/pong.c
	$(CC) $(CPPFLAGS) $(CFLAGS) src/pong.c $(LDFLAGS) $(LDLIBS) -o $@

run: pong
	./pong

life: game_of_life

game_of_life: src/game_of_life.c
	$(CC) $(CPPFLAGS) $(CFLAGS) src/game_of_life.c $(LDFLAGS) $(NCURSES_LIBS) $(LDLIBS) -o $@

run-life: game_of_life
	./game_of_life < patterns/glider.txt

test_game_of_life: tests/test_game_of_life.c src/game_of_life.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_game_of_life.c $(LDFLAGS) $(NCURSES_LIBS) $(LDLIBS) -o $@

test-life: game_of_life test_game_of_life
	./test_game_of_life
	$(PYTHON) tests/test_input.py
	$(PYTHON) tests/test_life_io.py

check-life:
	clang-format --dry-run --Werror src/game_of_life.c tests/test_game_of_life.c
	cppcheck --enable=all --inconclusive --check-level=exhaustive --std=c11 --suppress=missingIncludeSystem --error-exitcode=1 src/game_of_life.c

clean:
	rm -f pong game_of_life test_game_of_life
