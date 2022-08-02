#include <kernel.h>
#include <stdio.h>
#include <stdint.h>
#define ROUNDUP(a, sz)      ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))

typedef struct {
	void *start, *end;
} Area;
Area heap;
