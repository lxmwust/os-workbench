#include "co.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>
#include <assert.h>

#define Assert(x, s) \
	({ if (!x) { printf("%s", s); assert(x); } })

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

#define CO_STACK_SIZE (1 << 16)
enum co_status {
	CO_NEW = 1, // 新创建，还未执行过
	CO_RUNNING, // 已经执行过
	/* CO_WAITING, // co_wait等待中 */
	CO_DEAD,    // 已经结束，还未释放资源
};

struct co {
	int  id;					// co_pool id
	char *name;					// 协程名字
	void (*func)(void *);		// co_start 入口函数地址
	void *arg;					// 入口函数参数

	enum co_status	status;		// 协程状态
	/* struct co *		waiter; 	// 是否有其他协程在等待当前协程，如果有，优先执行 */
	jmp_buf			context;	// 协程栈的上下文
	uint8_t			stack[CO_STACK_SIZE];	// 协程栈
};

struct co *current;

#define CO_POOL_SIZE 128
struct co *co_pool[CO_POOL_SIZE];
int co_pool_add_idx, co_pool_yield_idx;

void co_pool_add(struct co *co) {
	int i, add_idx;
	for (i = 0; i < CO_POOL_SIZE; i ++ ) {
		add_idx = (co_pool_add_idx + i) % CO_POOL_SIZE;
		if (!co_pool[add_idx]) {
			co_pool_add_idx = add_idx;
			co_pool[add_idx] = co;
			co->id = add_idx;
			return;
		}
	}
	Assert((i == CO_POOL_SIZE), "Number of co has exceeded the upper limit\n");
}

void co_pool_remove(struct co *co) {
	co_pool[co->id] = NULL;
	co->id = -1;
}

static void co_run() {
	struct co *co = current;

	if (co->status == CO_NEW) {
		// run
		co->status = CO_RUNNING;
		co->func(co->arg);

		// finish
		co->status = CO_DEAD;

	}
	else if (co->status == CO_RUNNING) {
		longjmp(co->context, 1);
	}
}

void co_setCurrent() {
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
	struct co *co = (struct co *)malloc(sizeof(struct co));
	co->name = (char *)malloc(sizeof(name));
	strcpy(co->name, name);
	co->func = func;
	co->arg  = arg;

	co->status = CO_NEW;

	co_pool_add(co);

	return co;
}

// 1. main call
// 2. current == co
// 3. current != co
void co_wait(struct co *co) {
	Assert((co != NULL), "Shouldn't wait a non-existent co\n");

	int val = 0;
	jmp_buf waiter_buf;
	struct co *co_waiter = NULL; 

	// 保证co是current
	// 且co调用结束，会切回原current
	if (co != current) {
		co_waiter = current;
		current = co;
	}
	val = setjmp(waiter_buf);

	if (val == 0) {
		while (co->status != CO_DEAD) {
			co_run();
		}
		// change global env
		co_pool_remove(co);
		// current point to NULL;
		current = NULL;
		free(co);

		longjmp(waiter_buf, 1);
	}
	else {
		if (co_waiter != NULL) {
			current = co_waiter;
		}
	}
}

// 1. no more co
// 2. has other co
void co_yield() {
	Assert((current != NULL), "No co is running when calling co_yield");

	struct co *co_yielder = current;
	int val = setjmp(current->context);

	if (val == 0) {
		// call a co except the current
		int i, yield_idx;;
		for (i = 1; i < CO_POOL_SIZE; i ++ ) {
			yield_idx = (co_pool_yield_idx + i) % CO_POOL_SIZE;
			if (co_pool[yield_idx]) {
				co_pool_yield_idx = yield_idx;
				current = co_pool[yield_idx];
				co_run();
				break;
			}
		}
		if (co_yielder != NULL) {
			longjmp(co_yielder->context, 1);
		}
	}
}

__attribute__((constructor)) void co_pool_init() {
	for (int i = 0; i < CO_POOL_SIZE; i ++ ) {
		co_pool[i] = NULL;
	}
	co_pool_add_idx = co_pool_yield_idx = 0;

	current = NULL;
}
