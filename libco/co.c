#include "co.h"

#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct co *current;

enum co_status {
  CO_NEW = 1,  // 新创建，还未执行过
  CO_RUNNING,  // 已经执行过
  CO_WAITING,  // 在 co_wait 上等待
  CO_DEAD,     // 已经结束，但还未释放资源
};

#define K 1024
#define STACK_SIZE (64 * K)

struct co {
  const char *name;
  void (*func)(void *);  // co_start 指定的入口地址和参数
  void *arg;

  enum co_status status;            // 协程的状态
  struct co *waiter;                // 是否有其他协程在等待当前协程
  jmp_buf context;                  // 寄存器现场 (setjmp.h)
  unsigned char stack[STACK_SIZE];  // 协程的堆栈
};

typedef struct CONODE {
  struct co *coroutine;

  struct CONODE *fd, *bk;
} CoNode;

static CoNode *co_node = NULL;
/*
 * 如果co_node == NULL，则创建一个新的双向循环链表即可，并返回
 * 如果co_node != NULL, 则在co_node和co_node->fd之间插入，仍然返回co_node的值
 */
static void co_node_insert(struct co *coroutine) {
  CoNode *victim = (CoNode *)malloc(sizeof(CoNode));
  assert(victim);

  victim->coroutine = coroutine;
  if (co_node == NULL) {
    victim->fd = victim->bk = victim;
    co_node = victim;
  } else {
    victim->fd = co_node->fd;
    victim->bk = co_node;
    victim->fd->bk = victim->bk->fd = victim;
  }
}

/*
 * 如果当前只剩node一个，则返回该一个
 * 否则，拉取当前co_node对应的协程，并沿着bk方向移动
 */
static CoNode *co_node_remove() {
  CoNode *victim = NULL;

  if (co_node == NULL) {
    return NULL;
  } else if (co_node->bk == co_node) {
    victim = co_node;
    co_node = NULL;
  } else {
    victim = co_node;

    co_node = co_node->bk;
    co_node->fd = victim->fd;
    co_node->fd->bk = co_node;
  }

  return victim;
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  struct co *coroutine = (struct co *)malloc(sizeof(struct co));
  assert(coroutine);

  coroutine->name = name;
  coroutine->func = func;
  coroutine->arg = arg;
  coroutine->status = CO_NEW;
  coroutine->waiter = NULL;

  co_node_insert(coroutine);
  return coroutine;
}

void co_wait(struct co *coroutine) {
  assert(coroutine);

  if (coroutine->status != CO_DEAD) {
    coroutine->waiter = current;
    current->status = CO_WAITING;
    co_yield ();
  }

  /*
   * 释放coroutine对应的CoNode
   */
  while (co_node->coroutine != coroutine) {
    co_node = co_node->bk;
  }

  assert(co_node->coroutine == coroutine);

  free(coroutine);
  free(co_node_remove());
}

/*
 * 切换栈，即让选中协程的所有堆栈信息在自己的堆栈中，而非调用者的堆栈。保存调用者需要保存的寄存器，并调用指定的函数
 */
static inline void stack_switch_call(void *sp, void *entry, void *arg) {
  asm volatile(
#if __x86_64__
      "movq %%rcx, 0(%0); movq %0, %%rsp; movq %2, %%rdi; call *%1"
      :
      : "b"((uintptr_t)sp - 16), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#else
      "movl %%ecx, 4(%0); movl %0, %%esp; movl %2, 0(%0); call *%1"
      :
      : "b"((uintptr_t)sp - 8), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#endif
  );
}
/*
 * 从调用的指定函数返回，并恢复相关的寄存器。此时协程执行结束，以后再也不会执行该协程的上下文。这里需要注意的是，其和上面并不是对称的，因为调用协程给了新创建的选中协程的堆栈，则选中协程以后就在自己的堆栈上执行，永远不会返回到调用协程的堆栈。
 */
static inline void restore_return() {
  asm volatile(
#if __x86_64__
      "movq 0(%%rsp), %%rcx"
      :
      :
#else
      "movl 4(%%esp), %%ecx"
      :
      :
#endif
  );
}

#define __LONG_JUMP_STATUS (1)
void co_yield () {
  int status = setjmp(current->context);
  if (!status) {
    //此时开始查找待选中的进程，因为co_node应该指向的就是current对应的节点，因此首先向下移动一个，使当前线程优先级最低
    co_node = co_node->bk;
    while (!((current = co_node->coroutine)->status == CO_NEW || current->status == CO_RUNNING)) {
      co_node = co_node->bk;
    }

    assert(current);

    if (current->status == CO_RUNNING) {
      longjmp(current->context, __LONG_JUMP_STATUS);
    } else {
      ((struct co volatile *)current)->status = CO_RUNNING;  //这里如果直接赋值，编译器会和后面的覆写进行优化

      // 栈由高地址向低地址生长
      stack_switch_call(current->stack + STACK_SIZE, current->func, current->arg);
      //恢复相关寄存器
      restore_return();

      //此时协程已经完成执行
      current->status = CO_DEAD;

      if (current->waiter) {
        current->waiter->status = CO_RUNNING;
      }
      co_yield ();
    }
  }

  assert(status && current->status == CO_RUNNING);  //此时一定是选中的进程通过longjmp跳转到的情况执行到这里
}

static __attribute__((constructor)) void co_constructor(void) {
  current = co_start("main", NULL, NULL);
  current->status = CO_RUNNING;
}

static __attribute__((destructor)) void co_destructor(void) {
  if (co_node == NULL) {
    return;
  }

  while (co_node) {
    current = co_node->coroutine;
    free(current);
    free(co_node_remove());
  }
}