#include "dccthread.h"
#include "dlist.h"
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dccthread {
  ucontext_t context;
  char name[DCCTHREAD_MAX_NAME_SIZE];
};

struct dlist *threads;
ucontext_t managerContext;
dccthread_t *currentThread;
int stackSize = 0;

void managerFunction(int running) {
  while (running) {
    dccthread_t *thread = dlist_pop_left(threads);
    currentThread = thread;
    
    getcontext(&managerContext);
    setcontext(&thread->context);
  }
}

void dccthread_init(void (*func)(int), int param) {
  printf("init");

  
}

dccthread_t * dccthread_create(const char *name, void (*func)(int ), int param) {
  if (stackSize >= THREAD_STACK_SIZE) {
    printf("Thread stack is full.\n");
  } else {
    dccthread_t *newThread = malloc(sizeof(dccthread_t));
    if(!newThread) {
      printf("Failed to create a new thread.\n");
      exit(1);
    }

    getcontext(&newThread->context);
    makecontext(&newThread->context, func, 1, param);

    strncpy(newThread->name, name, sizeof(newThread->name));
    newThread->name[sizeof(newThread->name) - 1] = '\0';

    dlist_push_right(threads, newThread);

    stackSize += 1;

    return newThread;
  }
}

void dccthread_yield(void) {
  getcontext(&currentThread->context);

  dlist_push_right(threads, currentThread);

  setcontext(&managerContext);
};