#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

enum {
    MIN_COLUMNS = 60,
    MIN_ROWS = 20,
    MAX_WIDTH = 96,
    MAX_HEIGHT = 28,
    PADDLE_SIZE = 4,
    WIN_SCORE = 7
};

typedef struct {
    int width, height;
    int left_score, right_score;
    unsigned int serves;
    double left_y, right_y;
    double ball_x, ball_y, vx, vy;
    double serve_delay;
    bool two_players, paused, game_over;
} Game;

static struct termios original_terminal;
static bool terminal_active;
static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t suspend_requested;

static double clamp(double value, double low, double high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void restore_terminal(void) {
    if (!terminal_active) return;
    while (tcsetattr(STDIN_FILENO, TCSANOW, &original_terminal) == -1 &&
           errno == EINTR) {}
    fputs("\033[0m\033[?25h\033[?1049l", stdout);
    fflush(stdout);
    terminal_active = false;
}

static bool enter_terminal(void) {
    struct termios game_terminal = original_terminal;
    game_terminal.c_lflag &= (tcflag_t)~(ICANON | ECHO | IEXTEN);
    game_terminal.c_iflag &= (tcflag_t)~(IXON | ICRNL);
    game_terminal.c_oflag &= (tcflag_t)~OPOST;
    game_terminal.c_cc[VMIN] = 1;
    game_terminal.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &game_terminal) == -1) return false;
    terminal_active = true;
    fputs("\033[?1049h\033[?25l\033[2J\033[H", stdout);
    return fflush(stdout) != EOF;
}

static void on_signal(int number) {
    if (number == SIGTSTP) suspend_requested = 1;
    else stop_requested = 1;
}

static bool install_signal_handlers(void) {
    const int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGPIPE, SIGTSTP};
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        if (sigaction(signals[i], &action, NULL) == -1) return false;
    }
    return true;
}

static double monotonic_seconds(void) {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

static void serve(Game *game, int direction) {
    game->ball_x = (game->width - 1) / 2.0;
    game->ball_y = (game->height - 1) / 2.0;
    game->vx = direction * 18.0;
    game->vy = (game->serves++ % 2 == 0) ? 5.0 : -5.0;
    game->serve_delay = 0.8;
}

static void restart(Game *game) {
    game->left_score = game->right_score = 0;
    game->left_y = game->right_y = (game->height - PADDLE_SIZE) / 2.0;
    game->serves = 0;
    game->paused = game->game_over = false;
    serve(game, -1);
}

static bool resize_game(Game *game) {
    struct winsize size;
    int columns = 80, rows = 24;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
        if (size.ws_col != 0) columns = size.ws_col;
        if (size.ws_row != 0) rows = size.ws_row;
    }
    if (columns < MIN_COLUMNS || rows < MIN_ROWS) return false;
    int width = columns - 4;
    int height = rows - 6;
    if (width > MAX_WIDTH) width = MAX_WIDTH;
    if (height > MAX_HEIGHT) height = MAX_HEIGHT;
    if (game->width != width || game->height != height) {
        game->width = width;
        game->height = height;
        game->left_y = game->right_y = (height - PADDLE_SIZE) / 2.0;
        serve(game, (game->serves % 2 == 0) ? -1 : 1);
    }
    return true;
}

static void move_paddle(Game *game, bool right, int direction) {
    if (game->paused || game->game_over) return;
    double *position = right ? &game->right_y : &game->left_y;
    *position = clamp(*position + direction * 1.5, 0,
                      game->height - PADDLE_SIZE);
}

static void handle_input(Game *game, const char *input, size_t count,
                         int *escape_state) {
    for (size_t i = 0; i < count; ++i) {
        unsigned char key = (unsigned char)input[i];
        if (*escape_state == 1) {
            *escape_state = 0;
            if (key == '[' || key == 'O') {
                *escape_state = 2;
                continue;
            }
        } else if (*escape_state == 2) {
            if (key >= 0x40 && key <= 0x7e) {
                if (key == 'A' || key == 'B') {
                    move_paddle(game, game->two_players, key == 'A' ? -1 : 1);
                }
                *escape_state = 0;
            }
            continue;
        }
        switch (key) {
            case 27: *escape_state = 1; break;
            case 'w': case 'W': move_paddle(game, false, -1); break;
            case 's': case 'S': move_paddle(game, false, 1); break;
            case ' ': if (!game->game_over) game->paused = !game->paused; break;
            case 'r': case 'R': restart(game); break;
            case 'q': case 'Q': stop_requested = 1; break;
            default: break;
        }
    }
}

static bool paddle_hit(double ball_y, double paddle_y) {
    return ball_y >= paddle_y - 0.5 &&
           ball_y <= paddle_y + PADDLE_SIZE - 0.5;
}

static void bounce(Game *game, double paddle_y, int direction) {
    double speed = game->vx < 0 ? -game->vx : game->vx;
    double offset = (game->ball_y - paddle_y - (PADDLE_SIZE - 1) / 2.0) /
                    (PADDLE_SIZE / 2.0);
    game->vx = direction * clamp(speed * 1.06, 18.0, 32.0);
    game->vy = clamp(offset, -1, 1) * 12.0;
}

static void update(Game *game, double elapsed) {
    if (game->paused || game->game_over) return;
    if (game->serve_delay > 0) {
        game->serve_delay -= elapsed;
        return;
    }
    /* Small steps keep fast balls from jumping through a paddle. */
    while (elapsed > 0) {
        double dt = elapsed > 1.0 / 120.0 ? 1.0 / 120.0 : elapsed;
        elapsed -= dt;
        if (!game->two_players) {
            double target = game->vx > 0
                ? game->ball_y - (PADDLE_SIZE - 1) / 2.0
                : (game->height - PADDLE_SIZE) / 2.0;
            game->right_y = clamp(game->right_y +
                clamp(target - game->right_y, -7.5 * dt, 7.5 * dt),
                0, game->height - PADDLE_SIZE);
        }
        double previous_x = game->ball_x;
        game->ball_x += game->vx * dt;
        game->ball_y += game->vy * dt;
        if (game->ball_y < 0) {
            game->ball_y = -game->ball_y;
            game->vy = -game->vy;
        } else if (game->ball_y > game->height - 1) {
            game->ball_y = 2 * (game->height - 1) - game->ball_y;
            game->vy = -game->vy;
        }
        if (game->vx < 0 && previous_x >= 1 && game->ball_x <= 1 &&
            paddle_hit(game->ball_y, game->left_y)) {
            game->ball_x = 2 - game->ball_x;
            bounce(game, game->left_y, 1);
        } else if (game->vx > 0 && previous_x <= game->width - 2 &&
                   game->ball_x >= game->width - 2 &&
                   paddle_hit(game->ball_y, game->right_y)) {
            game->ball_x = 2 * (game->width - 2) - game->ball_x;
            bounce(game, game->right_y, -1);
        }
        if (game->ball_x < 0 || game->ball_x > game->width - 1) {
            bool left_scored = game->ball_x > game->width - 1;
            if (left_scored) ++game->left_score;
            else ++game->right_score;
            game->game_over = game->left_score >= WIN_SCORE ||
                              game->right_score >= WIN_SCORE;
            serve(game, left_scored ? 1 : -1);
            return;
        }
    }
}

static void draw_border(int width) {
    fputs("\033[2K+", stdout);
    for (int x = 0; x < width; ++x) putchar('-');
    fputs("+\r\n", stdout);
}

static bool render(const Game *game, bool enough_room) {
    fputs("\033[H", stdout);
    if (!enough_room) {
        fputs("\033[2KResize to 60x20\r\n\033[2KQ quits\033[J", stdout);
        return fflush(stdout) != EOF;
    }
    printf("\033[2K\033[1;36m PONG  %s %d : %d %s   First to %d\033[0m\r\n",
           game->two_players ? "P1" : "You", game->left_score,
           game->right_score, game->two_players ? "P2" : "CPU", WIN_SCORE);
    draw_border(game->width);
    int ball_x = (int)(game->ball_x + 0.5);
    int ball_y = (int)(game->ball_y + 0.5);
    int left_y = (int)(game->left_y + 0.5);
    int right_y = (int)(game->right_y + 0.5);
    for (int y = 0; y < game->height; ++y) {
        char line[MAX_WIDTH + 1];
        memset(line, ' ', (size_t)game->width);
        line[game->width] = '\0';
        if (y % 2 == 0) line[game->width / 2] = ':';
        if (y >= left_y && y < left_y + PADDLE_SIZE) line[1] = '#';
        if (y >= right_y && y < right_y + PADDLE_SIZE) line[game->width - 2] = '#';
        if (y == ball_y && ball_x >= 0 && ball_x < game->width) line[ball_x] = 'O';
        printf("\033[2K|%s|\r\n", line);
    }
    draw_border(game->width);
    const char *status = "";
    if (game->game_over) {
        status = game->left_score >= WIN_SCORE
            ? (game->two_players ? "Player 1 wins! R restarts." : "You win! R restarts.")
            : (game->two_players ? "Player 2 wins! R restarts." : "CPU wins! R restarts.");
    } else if (game->paused) status = "Paused - Space resumes.";
    else if (game->serve_delay > 0) status = "Get ready...";
    printf("\033[2K %s\r\n\033[2K %s\033[J", status, game->two_players
        ? "P1 W/S | P2 Up/Down | Space pause | R restart | Q quit"
        : "W/S or Up/Down move | Space pause | R restart | Q quit");
    return fflush(stdout) != EOF;
}

static void usage(const char *program) {
    printf("Usage: %s [--two-players | --help]\n"
           "Terminal Pong. Play against the CPU, or a friend with --two-players.\n"
           "W/S: left paddle. Arrow Up/Down: right paddle in two-player mode.\n"
           "Space: pause. R: restart. Q or Ctrl+C: quit. First to 7 wins.\n"
           "Requires an ANSI terminal of at least 60 columns and 20 rows.\n", program);
}

int main(int argc, char **argv) {
    Game game = {0};
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--two-players") == 0) {
        game.two_players = true;
    } else if (argc != 1) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fputs("Run Pong in an interactive terminal (without pipes or redirection).\n", stderr);
        return EXIT_FAILURE;
    }
    const char *term = getenv("TERM");
    if (term != NULL && strcmp(term, "dumb") == 0) {
        fputs("Pong needs an ANSI-compatible terminal. Use Terminal or iTerm2.\n", stderr);
        return EXIT_FAILURE;
    }
    if (tcgetattr(STDIN_FILENO, &original_terminal) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }
    if (atexit(restore_terminal) != 0 || !install_signal_handlers()) {
        fputs("Could not initialize terminal cleanup.\n", stderr);
        return EXIT_FAILURE;
    }
    if (!enter_terminal()) {
        perror("Could not enter game mode");
        return EXIT_FAILURE;
    }
    bool enough_room = resize_game(&game);
    if (enough_room) restart(&game);
    double previous_time = monotonic_seconds();
    int escape_state = 0;
    int result = EXIT_SUCCESS;
    while (!stop_requested) {
        if (suspend_requested) {
            restore_terminal();
            suspend_requested = 0;
            raise(SIGSTOP);
            if (stop_requested) break;
            if (!enter_terminal()) {
                result = EXIT_FAILURE;
                break;
            }
            previous_time = monotonic_seconds();
        }
        double now = monotonic_seconds();
        double elapsed = clamp(now - previous_time, 0, 0.05);
        previous_time = now;
        enough_room = resize_game(&game);
        if (enough_room) update(&game, elapsed);
        if (!render(&game, enough_room)) {
            result = EXIT_FAILURE;
            break;
        }
        fd_set input;
        FD_ZERO(&input);
        FD_SET(STDIN_FILENO, &input);
        struct timeval timeout = {0, 16667};
        int ready = select(STDIN_FILENO + 1, &input, NULL, NULL, &timeout);
        if (ready < 0) {
            if (errno == EINTR) continue;
            result = EXIT_FAILURE;
            break;
        }
        if (ready > 0) {
            char keys[128];
            ssize_t count = read(STDIN_FILENO, keys, sizeof(keys));
            if (count == 0) break;
            if (count < 0) {
                if (errno == EINTR) continue;
                result = EXIT_FAILURE;
                break;
            }
            handle_input(&game, keys, (size_t)count, &escape_state);
        }
    }
    return result;
}
