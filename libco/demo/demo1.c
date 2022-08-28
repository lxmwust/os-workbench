#include <stdio.h>
#include <stdint.h>
#include <setjmp.h>

jmp_buf buf;

void add(int p) {
	if (p >= 10000) return;
	int n = 0;
	add(p + 1);
}

void foo() {
	add(0);
	/* printf("%d" ,1); */

	/* asm("movq %0, %%rsp;" : : "b"(sp) : "memory"); */
}

int main() {
	int stack[1 << 20];

	if (setjmp(buf) == 0) {
#ifdef __x86_64__
		asm("movq %0, %%rsp; jmp *%1;" : : "b"((uintptr_t)stack), "d"(foo) : "memory");
#endif
		longjmp(buf, 1);
	}
}
