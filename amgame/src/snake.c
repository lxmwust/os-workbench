#include <snake.h>
#include <am.h>
#include <com.h>
#include <stdio.h>

static int w, h;

static void init() {
	AM_GPU_CONFIG_T info = {0};
	ioe_read(AM_GPU_CONFIG, &info);
	w = info.width;
	h = info.height;
}

void sk_init(Snake *sk) { 
	init();

	sk->size = SNAKE_INIT_SIZE;
	for (int i = 0; i < sk->size; i++) {
		sk->body[i].pos = (Position){ 0, i };
	}
	sk->dir = (Direction){ NONE, DOWN };
}

int sk_conflict(Snake *sk, Direction dir) {
	int id = sk->size - 1;
	int x = sk->body[id].pos.x + dir.x;
	int y = sk->body[id].pos.y + dir.y;

	// conflict with bound
	if (x < 0 || x * GRID_SIZE >= w - SNAKE_SIZE ||
		y < 0 || y * GRID_SIZE >= h - SNAKE_SIZE)
		return 2;

	if (id == 0) return 0;

	// conflict with body
	if (x == sk->body[id - 1].pos.x &&
		y == sk->body[id - 1].pos.y)
		return 1;
	return 0;
}

void sk_move(Snake *sk) {
	for (int i = 0; i < sk->size - 1; i++) {
		sk->body[i].pos.x = sk->body[i + 1].pos.x;
		sk->body[i].pos.y = sk->body[i + 1].pos.y;
	}
	sk->body[sk->size - 1].pos.x += sk->dir.x;
	sk->body[sk->size - 1].pos.y += sk->dir.y;
}
