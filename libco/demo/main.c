#include <stdio.h>
#include <stdint.h>
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
uintptr_t sp;

void foo(void *arg) {
	printf("Hello World\n");
	/* printf("%s\n", (const char *)arg); */
	longjmp(buf, 1);
}

int main() {
	int stack[1 << 20];
	char s[10] = { 'H', 'i', '\0' };

	if (setjmp(buf) == 0) {
		stack_switch_call(stack, foo, (uintptr_t)s);
	}
}
