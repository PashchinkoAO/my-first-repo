#include <stdio.h>

#define WIDTH 80
#define HEIGHT 25
#define PADDLE_SIZE 3
#define WIN_SCORE 21

void draw_field(int left_y, int right_y, int ball_x, int ball_y, int left_score, int right_score);
int read_command(void);
int move_paddle(int y, int command, int up, int down);
int bounce_vertical(int y, int dy);
int bounce_horizontal(int x, int next_y, int dx, int left_y, int right_y);

int main(void) {
    int left_y = HEIGHT / 2 - PADDLE_SIZE / 2;
    int right_y = left_y;
    int ball_x = WIDTH / 2, ball_y = HEIGHT / 2;
    int dx = 1, dy = 1;
    int left_score = 0, right_score = 0;
    int command = 0;
    draw_field(left_y, right_y, ball_x, ball_y, left_score, right_score);
    while (left_score < WIN_SCORE && right_score < WIN_SCORE && command != EOF) {
        command = read_command();
        if (command != EOF) {
            left_y = move_paddle(left_y, command, 'a', 'z');
            right_y = move_paddle(right_y, command, 'k', 'm');
            dy = bounce_vertical(ball_y, dy);
            dx = bounce_horizontal(ball_x, ball_y + dy, dx, left_y, right_y);
            ball_x += dx;
            ball_y += dy;
            if (ball_x == 1 || ball_x == WIDTH - 2) {
                if (ball_x == 1) {
                    right_score++;
                } else {
                    left_score++;
                }
                ball_x = WIDTH / 2;
                ball_y = HEIGHT / 2;
                dy = 1;
            }
            draw_field(left_y, right_y, ball_x, ball_y, left_score, right_score);
        }
    }
    if (left_score == WIN_SCORE) {
        printf("Congratulations, player 1! You won!\n");
    } else if (right_score == WIN_SCORE) {
        printf("Congratulations, player 2! You won!\n");
    }
    return 0;
}

void draw_field(int left_y, int right_y, int ball_x, int ball_y, int left_score, int right_score) {
    printf("\nPlayer 1: %d  Player 2: %d\n", left_score, right_score);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (y == 0 || y == HEIGHT - 1) {
                putchar('-');
            } else if (x == 0 || x == WIDTH - 1) {
                putchar('|');
            } else if (x == ball_x && y == ball_y) {
                putchar('O');
            } else if ((x == 2 && y >= left_y && y < left_y + PADDLE_SIZE) ||
                       (x == WIDTH - 3 && y >= right_y && y < right_y + PADDLE_SIZE)) {
                putchar('|');
            } else if (x == WIDTH / 2) {
                putchar(':');
            } else {
                putchar(' ');
            }
        }
        putchar('\n');
    }
    printf("A/Z: player 1 | K/M: player 2 | Space: skip. Press Enter.\n");
    fflush(stdout);
}

int read_command(void) {
    int command = 0;
    int correct = 0;
    while (!correct && command != EOF) {
        command = getchar();
        if (command != EOF && command != '\n') {
            int next = getchar();
            int too_long = 0;
            while (next != '\n' && next != EOF) {
                too_long = 1;
                next = getchar();
            }
            if (command >= 'A' && command <= 'Z') {
                command = command - 'A' + 'a';
            }
            correct = !too_long && (command == 'a' || command == 'z' || command == 'k' ||
                                    command == 'm' || command == ' ');
        }
        if (!correct && command != EOF) {
            printf("Enter one command: A, Z, K, M or Space, then Enter.\n");
            fflush(stdout);
        }
    }
    return command;
}

int move_paddle(int y, int command, int up, int down) {
    if (command == up && y > 1) {
        y--;
    } else if (command == down && y < HEIGHT - PADDLE_SIZE - 1) {
        y++;
    }
    return y;
}

int bounce_vertical(int y, int dy) {
    if (y + dy == 0 || y + dy == HEIGHT - 1) {
        dy = -dy;
    }
    return dy;
}

int bounce_horizontal(int x, int next_y, int dx, int left_y, int right_y) {
    if (dx == -1 && x + dx == 2 && next_y >= left_y && next_y < left_y + PADDLE_SIZE) {
        dx = -dx;
    } else if (dx == 1 && x + dx == WIDTH - 3 && next_y >= right_y &&
               next_y < right_y + PADDLE_SIZE) {
        dx = -dx;
    }
    return dx;
}
