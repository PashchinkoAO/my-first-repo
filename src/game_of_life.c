#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <term.h>

#define WIDTH 80
#define HEIGHT 25
/* Задержка измеряется в миллисекундах: меньше задержка — выше скорость. */
#define INITIAL_DELAY 250
#define MIN_DELAY 50
#define MAX_DELAY 1000
#define DELAY_STEP 50

/* Объявления позволяют читать файл сверху вниз, начиная с main. */
static int read_field(int field[HEIGHT][WIDTH]);
static int read_cell(void);
static int check_input_end(void);
static int terminal_supported(void);
static int start_game(int field[HEIGHT][WIDTH]);
static void run_game(int field[HEIGHT][WIDTH]);
static void draw_screen(int field[HEIGHT][WIDTH], int delay);
static int terminal_fits(void);
static int change_delay(int delay, int change);
static void next_generation(int field[HEIGHT][WIDTH], int next[HEIGHT][WIDTH]);
static int count_neighbors(int field[HEIGHT][WIDTH], int row, int column);
static void copy_field(int destination[HEIGHT][WIDTH], int source[HEIGHT][WIDTH]);

int main(void) {
    /* 0 — мёртвая клетка, 1 — живая. Оба индекса начинаются с нуля. */
    int field[HEIGHT][WIDTH] = {0};
    int result = 1;
    if (read_field(field)) {
        result = start_game(field);
    } else {
        fprintf(stderr, "Expected exactly 2000 cells: 0 or 1, with optional whitespace.\n");
    }
    return result;
}

static int read_field(int field[HEIGHT][WIDTH]) {
    int valid = 1;
    for (int row = 0; row < HEIGHT && valid; row++) {
        for (int column = 0; column < WIDTH && valid; column++) {
            int cell = read_cell();
            if (cell == '0' || cell == '1') {
                /* Символ '1' из файла превращается в число 1 в массиве. */
                field[row][column] = cell - '0';
            } else {
                valid = 0;
            }
        }
    }
    if (valid) {
        valid = check_input_end();
    }
    return valid;
}

static int read_cell(void) {
    int cell = getchar();
    /* Пробелы и переводы строк разделяют клетки, но не являются клетками. */
    while (cell != EOF && isspace(cell)) {
        cell = getchar();
    }
    return cell;
}

static int check_input_end(void) {
    int valid = 1;
    int cell = getchar();
    while (cell != EOF && valid) {
        if (isspace(cell)) {
            cell = getchar();
        } else {
            valid = 0;
        }
    }
    if (ferror(stdin)) {
        valid = 0;
    }
    return valid;
}

static int start_game(int field[HEIGHT][WIDTH]) {
    int result = 1;
    /* stdin занят файлом поля; /dev/tty открывает клавиатуру терминала. */
    FILE *keyboard = fopen("/dev/tty", "r");
    if (keyboard != NULL) {
        if (terminal_supported()) {
            /* ncurses выводит кадры в stdout, а клавиши читает из keyboard. */
            SCREEN *screen = newterm(NULL, stdout, keyboard);
            if (screen != NULL) {
                int previous_cursor = curs_set(0);
                if (cbreak() != ERR && noecho() != ERR) {
                    run_game(field);
                    result = 0;
                }
                if (previous_cursor != ERR) {
                    curs_set(previous_cursor);
                }
                /* Восстанавливаем терминал и освобождаем ресурсы библиотеки. */
                endwin();
                delscreen(screen);
            }
        }
        fclose(keyboard);
    }
    if (result != 0) {
        fprintf(stderr, "Cannot initialize ncurses. Run in a terminal with a valid TERM.\n");
    }
    return result;
}

static int terminal_supported(void) {
    int error = 0;
    /* Проверяем TERM до создания экрана; 1 — дескриптор стандартного вывода. */
    int valid = setupterm(NULL, 1, &error) == OK;
    if (valid) {
        del_curterm(cur_term);
    }
    return valid;
}

static void run_game(int field[HEIGHT][WIDTH]) {
    int next[HEIGHT][WIDTH] = {0};
    int delay = INITIAL_DELAY;
    int running = 1;
    while (running) {
        draw_screen(field, delay);
        /* getch ждёт клавишу не дольше delay миллисекунд. */
        timeout(delay);
        int key = getch();
        if (key == ' ') {
            running = 0;
        } else if (key == 'a' || key == 'A') {
            delay = change_delay(delay, -DELAY_STEP);
        } else if (key == 'z' || key == 'Z') {
            delay = change_delay(delay, DELAY_STEP);
        } else if (key == ERR && terminal_fits()) {
            next_generation(field, next);
            copy_field(field, next);
        }
    }
}

static void draw_screen(int field[HEIGHT][WIDTH], int delay) {
    erase();
    if (terminal_fits()) {
        for (int row = 0; row < HEIGHT; row++) {
            for (int column = 0; column < WIDTH; column++) {
                mvaddch(row, column, field[row][column] ? 'O' : '.');
            }
        }
        mvprintw(HEIGHT, 0, "A: faster | Z: slower | Space: exit | Delay: %d ms", delay);
    } else {
        /* В маленьком окне эволюция приостановлена, но выход доступен. */
        mvaddnstr(0, 0, "Resize to at least 80 columns x 26 rows. Space: exit.", getmaxx(stdscr) - 1);
    }
    refresh();
}

static int terminal_fits(void) {
    int rows = 0;
    int terminal_columns = 0;
    getmaxyx(stdscr, rows, terminal_columns);
    return rows >= HEIGHT + 1 && terminal_columns >= WIDTH;
}

static int change_delay(int delay, int change) {
    delay += change;
    if (delay < MIN_DELAY) {
        delay = MIN_DELAY;
    } else if (delay > MAX_DELAY) {
        delay = MAX_DELAY;
    }
    return delay;
}

static void next_generation(int field[HEIGHT][WIDTH], int next[HEIGHT][WIDTH]) {
    /* Читаем только field, пишем только next: поколение меняется целиком. */
    for (int row = 0; row < HEIGHT; row++) {
        for (int column = 0; column < WIDTH; column++) {
            int neighbors = count_neighbors(field, row, column);
            /* Рождение при трёх соседях; выживание живой клетки при двух. */
            next[row][column] = neighbors == 3 || (field[row][column] && neighbors == 2);
        }
    }
}

static int count_neighbors(int field[HEIGHT][WIDTH], int row, int column) {
    int neighbors = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            /* При dy == 0 и dx == 0 мы попали в саму клетку, а не в соседа. */
            if (dy != 0 || dx != 0) {
                /* Добавление размера убирает отрицательный остаток в C. */
                int neighbor_row = (row + dy + HEIGHT) % HEIGHT;
                int neighbor_column = (column + dx + WIDTH) % WIDTH;
                neighbors += field[neighbor_row][neighbor_column];
            }
        }
    }
    return neighbors;
}

static void copy_field(int destination[HEIGHT][WIDTH], int source[HEIGHT][WIDTH]) {
    for (int row = 0; row < HEIGHT; row++) {
        for (int column = 0; column < WIDTH; column++) {
            destination[row][column] = source[row][column];
        }
    }
}
