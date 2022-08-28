#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) {
	asm volatile (
#if __x86_64__
		"movq %0, %%rsp; movq %2, %%rdi; jmp *%1"
			: : "b"((uintptr_t)sp), "d"(entry), "a"(arg) : "memory"
#else
		"movl %0, %%esp; movl %2, 4(%0); jmp *%1"
			: : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg) : "memory"
#endif
	);
}
jmp_buf buf;

struct arg {
	void (*func)();
	int a;
};
struct arg garg;

void print() {
	printf("func: print\n");
}

void foo(void *arg) {
	struct arg *parg = (struct arg*)arg;

	/* garg.func = parg->func; */
	printf(" ");
	/* garg.a = parg->a; */

	longjmp(buf, 1);
}

int main() {
	uint8_t stack[1 << 20];
	struct arg arg = { print, 1 };
	
	if (!setjmp(buf)) {
		stack_switch_call(stack, foo, (uintptr_t)&arg); 
	}
	/* garg.func(); */
	/* printf("%d\n", garg.a); */
	
	return 0;
}
