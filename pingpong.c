#include <stdio.h>

// Координаты ракеток задают их середину. 
// Каждая ракетка занимает три строки: y - 1, y, y + 1.
void draw_field(int left_paddle_y, int right_paddle_y,
                int ball_x, int ball_y) {
  // y — номер строки сверху вниз, от 0 до 24.
  // x — номер столбца слева направо, от 0 до 79.
  for (int y = 0; y < 25; y++) {
    for (int x = 0; x < 80; x++) {
      if (y == 0 || y == 24) {
        // Верхняя и нижняя границы поля.
        putchar('-');
      } else if (x == 0 || x == 79) {
        // Левая и правая границы поля.
        putchar('|');
      } else if (x == ball_x && y == ball_y) {
        // Мяч рисуем раньше центральной линии,
        // чтобы на пересечении было видно 'O', а не ':'.
        putchar('O');
      } else if (x == 39) {
        // Центральная линия поля.
        putchar(':');
      } else if (x == 3 &&
                 y >= left_paddle_y - 1 &&
                 y <= left_paddle_y + 1) {
        // Левая ракетка: столбец 3, три строки вокруг её середины.
        putchar('#');
      } else if (x == 76 &&
                 y >= right_paddle_y - 1 &&
                 y <= right_paddle_y + 1) {
        // Правая ракетка: столбец 76, три строки вокруг её середины.
        putchar('#');
      } else {
        // Всё остальное — пустое место.
        putchar(' ');
      }
    }

    // После 80 символов текущей строки переходим на следующую.
    putchar('\n');
  }
}

// Возвращает новую координату середины одной ракетки.
// command — символ, который нажал игрок.
// paddle_y — текущая координата середины ракетки по вертикали.
int move_paddle(int command, int paddle_y) {
  switch (command) {
  case 'a':
  case 'k':
    // Движение вверх.
    // Середина не может подняться выше y = 2:
    // иначе верхний символ ракетки попал бы в строку 1,
    // а строка 0 — это граница поля.
    if (paddle_y > 2) {
      paddle_y--;
    }
    break;

  case 'z':
  case 'm':
    // Движение вниз.
    // Середина не может опуститься ниже y = 22:
    // иначе нижний символ ракетки попал бы в строку 23,
    // а строка 24 — это граница поля.
    if (paddle_y < 22) {
      paddle_y++;
    }
    break;
  }

  return paddle_y;
}

int main(void) {
  // Начальные положения: обе ракетки и мяч — в центре поля.
  int left_paddle_y = 12;
  int right_paddle_y = 12;
  int ball_x = 39;
  int ball_y = 12;

  // Направление мяча по x:  1 — вправо, -1 — влево.
  // Направление мяча по y:  1 — вниз,   -1 — вверх.
  int ball_direction_x = 1;
  int ball_direction_y = 1;

  int left_score = 0;
  int right_score = 0;

  printf("Score: %d : %d\n", left_score, right_score);
  draw_field(left_paddle_y, right_paddle_y, ball_x, ball_y);

  // Игра идёт, пока никто не набрал 21 очко.
  while (left_score < 21 && right_score < 21) {
    int command = getchar();

    switch (command) {
    case 'a':
    case 'z':
      // Управление левой ракеткой: 'a' — вверх, 'z' — вниз.
      left_paddle_y = move_paddle(command, left_paddle_y);
      break;

    case 'k':
    case 'm':
      // Управление правой ракеткой: 'k' — вверх, 'm' — вниз.
      right_paddle_y = move_paddle(command, right_paddle_y);
      break;

    case ' ':
      // Пробел: ракетки стоят на месте, но мяч всё равно делает ход.
      break;

    default:
      // Enter и любые другие символы не считаются игровым ходом.
      continue;
    }

    // Если на следующем шаге мяч упрётся в верхнюю (y = 0)
    // или нижнюю (y = 24) границу, заранее меняем направление по y.
    // Это делается до расчёта next_ball_y, чтобы дальнейшие проверки
    // работали с уже исправленным направлением.
    if (ball_y + ball_direction_y == 0 ||
        ball_y + ball_direction_y == 24) {
      ball_direction_y = -ball_direction_y;
    }

    // Считаем клетку, в которую мяч попадёт на следующем шаге.
    int next_ball_x = ball_x + ball_direction_x;
    int next_ball_y = ball_y + ball_direction_y;

    // Проверяем, не врежется ли мяч в левую ракетку.
    // Условия:
    //   1) мяч летит влево;
    //   2) следующая клетка по x — столбец 3 (там стоит ракетка);
    //   3) следующая клетка по y попадает в диапазон строк ракетки.
    if (ball_direction_x == -1 &&
        next_ball_x == 3 &&
        next_ball_y >= left_paddle_y - 1 &&
        next_ball_y <= left_paddle_y + 1) {
      ball_direction_x = -ball_direction_x;
    }
    // Иначе проверяем правую ракетку — по тем же правилам,
    // но столбец 76 и направление вправо.
    else if (ball_direction_x == 1 &&
             next_ball_x == 76 &&
             next_ball_y >= right_paddle_y - 1 &&
             next_ball_y <= right_paddle_y + 1) {
      ball_direction_x = -ball_direction_x;
    }

    // Двигаем мяч. Если было столкновение с ракеткой,
    // направление по x изменяется, и мяч уйдёт обратно.
    ball_x += ball_direction_x;
    ball_y += ball_direction_y;

    // Столбцы 2 и 77 находятся сразу за ракетками:
    // если мяч туда дошёл, значит, ракетка его не отбила - это гол.
    if (ball_x == 2 || ball_x == 77) {
      if (ball_x == 2) {
        // Мяч пролетел слева — очко правому игроку.
        right_score++;
      } else {
        // Мяч пролетел справа — очко левому игроку.
        left_score++;
      }

      // После гола возвращаем мяч и обе ракетки в центр.
      // Направление по x не сбрасываем:
      // подача идёт в сторону пропустившего игрока.
      ball_x = 39;
      ball_y = 12;
      ball_direction_y = 1;

      left_paddle_y = 12;
      right_paddle_y = 12;
    }

    printf("Score: %d : %d\n", left_score, right_score);
    draw_field(left_paddle_y, right_paddle_y, ball_x, ball_y);
  }

  // Игра закончилась: проверяем, кто набрал 21 очко.
  if (left_score == 21) {
    printf("Congratulations, player 1!\n");
  } else if (right_score == 21) {
    printf("Congratulations, player 2!\n");
  }

  return 0;
}
