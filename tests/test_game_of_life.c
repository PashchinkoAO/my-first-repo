#include <assert.h>
#include <string.h>

/* Keep the exercise itself in one source file while testing its functions. */
#define main game_of_life_main
#include "../src/game_of_life.c"
#undef main

void assert_fields_equal(int actual[HEIGHT][WIDTH], int expected[HEIGHT][WIDTH]) {
    for (int row = 0; row < HEIGHT; row++) {
        for (int column = 0; column < WIDTH; column++) {
            assert(actual[row][column] == expected[row][column]);
        }
    }
}

void advance(int field[HEIGHT][WIDTH], int generations) {
    int next[HEIGHT][WIDTH] = {0};
    for (int generation = 0; generation < generations; generation++) {
        next_generation(field, next);
        copy_field(field, next);
    }
}

void test_empty(void) {
    int field[HEIGHT][WIDTH] = {0};
    int next[HEIGHT][WIDTH] = {0};
    next_generation(field, next);
    assert_fields_equal(next, field);
}

void test_block(void) {
    int field[HEIGHT][WIDTH] = {0};
    int next[HEIGHT][WIDTH] = {0};
    field[10][20] = field[10][21] = 1;
    field[11][20] = field[11][21] = 1;
    next_generation(field, next);
    assert_fields_equal(next, field);
}

void test_blinker(void) {
    int field[HEIGHT][WIDTH] = {0};
    int original[HEIGHT][WIDTH] = {0};
    int expected[HEIGHT][WIDTH] = {0};
    int next[HEIGHT][WIDTH] = {0};
    field[12][39] = field[12][40] = field[12][41] = 1;
    expected[11][40] = expected[12][40] = expected[13][40] = 1;
    copy_field(original, field);
    next_generation(field, next);
    assert_fields_equal(next, expected);
    assert_fields_equal(field, original);
    copy_field(field, next);
    next_generation(field, next);
    assert_fields_equal(next, original);
}

void test_glider(void) {
    int field[HEIGHT][WIDTH] = {0};
    int expected[HEIGHT][WIDTH] = {0};
    field[8][11] = field[9][12] = 1;
    field[10][10] = field[10][11] = field[10][12] = 1;
    expected[9][12] = expected[10][13] = 1;
    expected[11][11] = expected[11][12] = expected[11][13] = 1;
    advance(field, 4);
    assert_fields_equal(field, expected);
}

void test_corner_neighbors(void) {
    int field[HEIGHT][WIDTH] = {0};
    field[0][0] = field[0][WIDTH - 1] = 1;
    field[HEIGHT - 1][0] = field[HEIGHT - 1][WIDTH - 1] = 1;
    assert(count_neighbors(field, 0, 0) == 3);
    assert(count_neighbors(field, 0, WIDTH - 1) == 3);
    assert(count_neighbors(field, HEIGHT - 1, 0) == 3);
    assert(count_neighbors(field, HEIGHT - 1, WIDTH - 1) == 3);
    assert(count_neighbors(field, 12, 40) == 0);
}

void test_eight_neighbors(void) {
    int field[HEIGHT][WIDTH] = {0};
    field[HEIGHT - 1][WIDTH - 1] = field[HEIGHT - 1][0] = field[HEIGHT - 1][1] = 1;
    field[0][WIDTH - 1] = field[0][1] = 1;
    field[1][WIDTH - 1] = field[1][0] = field[1][1] = 1;
    assert(count_neighbors(field, 0, 0) == 8);
    field[0][0] = 1;
    assert(count_neighbors(field, 0, 0) == 8);
}

void test_wrapped_blinker(void) {
    int field[HEIGHT][WIDTH] = {0};
    int expected[HEIGHT][WIDTH] = {0};
    int original[HEIGHT][WIDTH] = {0};
    field[0][WIDTH - 1] = field[0][0] = field[0][1] = 1;
    expected[HEIGHT - 1][0] = expected[0][0] = expected[1][0] = 1;
    copy_field(original, field);
    advance(field, 1);
    assert_fields_equal(field, expected);
    advance(field, 1);
    assert_fields_equal(field, original);
}

void test_birth_and_overpopulation(void) {
    int field[HEIGHT][WIDTH] = {0};
    int next[HEIGHT][WIDTH] = {0};
    int expected[HEIGHT][WIDTH] = {0};
    for (int row = 10; row <= 12; row++) {
        for (int column = 10; column <= 12; column++) {
            field[row][column] = 1;
        }
    }
    expected[10][10] = expected[10][12] = 1;
    expected[12][10] = expected[12][12] = 1;
    expected[9][11] = expected[13][11] = 1;
    expected[11][9] = expected[11][13] = 1;
    next_generation(field, next);
    assert_fields_equal(next, expected);
}

void test_copy(void) {
    int source[HEIGHT][WIDTH] = {0};
    int destination[HEIGHT][WIDTH] = {0};
    source[0][0] = source[HEIGHT - 1][WIDTH - 1] = 1;
    destination[12][40] = 1;
    copy_field(destination, source);
    assert_fields_equal(destination, source);
    source[0][0] = 0;
    assert(destination[0][0] == 1);
}

void test_speed_limits(void) {
    assert(change_delay(INITIAL_DELAY, -DELAY_STEP) == INITIAL_DELAY - DELAY_STEP);
    assert(change_delay(INITIAL_DELAY, DELAY_STEP) == INITIAL_DELAY + DELAY_STEP);
    assert(change_delay(MIN_DELAY, -DELAY_STEP) == MIN_DELAY);
    assert(change_delay(MAX_DELAY, DELAY_STEP) == MAX_DELAY);
    assert(change_delay(MIN_DELAY + 1, -DELAY_STEP) == MIN_DELAY);
    assert(change_delay(MAX_DELAY - 1, DELAY_STEP) == MAX_DELAY);
}

int check_input(void) {
    int field[HEIGHT][WIDTH] = {0};
    int status = 1;
    if (read_field(field)) {
        int alive = 0;
        for (int row = 0; row < HEIGHT; row++) {
            for (int column = 0; column < WIDTH; column++) {
                alive += field[row][column];
            }
        }
        printf("alive=%d\n", alive);
        status = 0;
    }
    return status;
}

int main(int argc, char **argv) {
    int status = 1;
    if (argc == 1) {
        test_empty();
        test_block();
        test_blinker();
        test_glider();
        test_corner_neighbors();
        test_eight_neighbors();
        test_wrapped_blinker();
        test_birth_and_overpopulation();
        test_copy();
        test_speed_limits();
        puts("PASS: 10 core checks (rules, simultaneous updates, torus, copy, speed).");
        status = 0;
    } else if (argc == 2 && strcmp(argv[1], "--read-field") == 0) {
        status = check_input();
    }
    return status;
}
