#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>
#include <snake.h>

// void splash();
void get_key(Direction *dir);

void draw_snake(Snake *sk);
void draw_snake_move(Snake *sk, Direction dir, int *game_state);
void draw_snake_clear(Snake *sk);
