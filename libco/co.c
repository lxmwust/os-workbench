#include "co.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define Assert(x, s) \
	do { if (!x) { printf("> co assert: %s", s); assert(x); } } while(0)

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

enum co_status {
	CO_NEW = 1,
	CO_RUNNING,
	CO_WAITING,
	CO_DEAD,
};
#define CO_STACK_SIZE (1 << 16)
struct co {
	int  id;
	char *name;
	void (*func)(void *);
	void *arg;

	enum co_status	status;
	struct co *		waiter;
	jmp_buf			context;
	uint8_t			stack[CO_STACK_SIZE];
};

#define CO_POOL_SIZE 128
static struct co *current;
static struct co *co_pool[CO_POOL_SIZE];
static int co_pool_size;
static void co_pool_init() {
	for (int i = 0; i < CO_POOL_SIZE; i ++ ) {
		co_pool[i] = NULL;
	}
	current = NULL;
}
static int co_pool_insert(struct co *co) {
	int success = 0;
	for (int i = 0; i < CO_POOL_SIZE; i ++ ) {
		if (co_pool[i] == NULL) {
			co_pool[i] = co;
			co->id = i;
			success = 1;
			co_pool_size ++ ;
			break;
		}
	}
	return success;
}
static struct co *co_pool_next() {
	int start = rand() % co_pool_size;
	for (int i = start; i < CO_POOL_SIZE + start; i ++ ) {
		struct co *next = co_pool[i % CO_POOL_SIZE];
		if (next == NULL)				continue;
		if (next->status == CO_DEAD)	continue;
		if (next->status == CO_WAITING)	continue;

		return next;
	}
	return NULL;
}
void co_free(struct co *co) {
	Assert((co != NULL), "should not free a NULL co pointer");

	co_pool[co->id] = NULL;
	co_pool_size -- ;

	if (!co->name) {
		free(co->name);
	}
	free(co);
}

static void co_main_init() {
	struct co *main = co_start("main", NULL, NULL);
	// main is just a concept to simulate the main function, not the real main
	main->status = CO_RUNNING;
	current = main;
}

static void co_switch_dead(struct co *co);
static void co_wrapper(void *arg) {
	struct co *co = (struct co *)arg;

	co->func(co->arg);
	co->status = CO_DEAD;
	co_switch_dead(co);
}

static void co_switch_new(struct co *co) {
	current = co;
	co->status = CO_RUNNING;
	stack_switch_call(co->stack + CO_STACK_SIZE, co_wrapper, (uintptr_t)co);
}
static void co_switch_running(struct co *co) {
	current = co;
	longjmp(co->context, 1);
}
static void co_switch_dead(struct co *co) {
	if (co->waiter != NULL) {
		longjmp(co->waiter->context, 1);
	}
	else {
		co_yield();
	}
}
static void co_switch_waiting(struct co *co) {
	Assert(0, "execute a waiting co. "
			  "maybe a waiting circle has occured\n");
}
typedef void (*co_handler_t)(void *arg);
static void *co_switch[] = {
	[CO_NEW]		= co_switch_new,
	[CO_RUNNING]	= co_switch_running,
	[CO_DEAD]	    = co_switch_dead,
	[CO_WAITING]	= co_switch_waiting,
};

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
	struct co *co = (struct co *)malloc(sizeof(struct co));
	
	co->name = (char *)malloc(sizeof(name));
	strcpy(co->name, name);
	co->func = func;
	co->arg  = arg;
	
	co->status = CO_NEW;
	co->waiter = NULL;

	co_pool_insert(co);

	return co;
}

void co_yield() {
	/* jmp_buf buf; */
	/* int val = setjmp(buf); */
	int val = setjmp(current->context);

	if (val == 0) {
		struct co* next = co_pool_next();
		Assert((next != NULL), "next co to execute shouldn't be empty");

		((co_handler_t)co_switch[next->status])(next);
	}
	else {
	}
}

void co_wait(struct co *co) {
	Assert((co != NULL), "wait a null co\n");
	Assert((co != current), "current co can't wait itself\n");

	int val = setjmp(current->context);

	if (val == 0) {
		current->status = CO_WAITING;
		co->waiter = current;
		((co_handler_t)co_switch[co->status])(co);
	}
	else {
		co_free(co);
		current->status = CO_RUNNING;
	}
}

__attribute__((constructor)) void co_constructor() {
	srand((unsigned int)time(NULL));
	co_pool_init();
	co_main_init();
}

__attribute__((destructor)) void co_destructor() {
	co_free(co_pool[0]);
}
