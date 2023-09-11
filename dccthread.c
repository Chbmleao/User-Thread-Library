#include "dccthread.h"
#include "dlist.h"
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dccthread {
  ucontext_t context;
  char name[DCCTHREAD_MAX_NAME_SIZE];
  void (*func) (int);
  int param;
};

struct dlist *threads;
dccthread_t *managerThread = NULL;
dccthread_t *currentThread = NULL;

void managerFunction(int running) {
  while (running) {
    if(!dlist_empty(threads)) {
      dccthread_t *thread = dlist_pop_left(threads);
      currentThread = thread;
      
      setcontext(&thread->context);
    } else {
      running = 0;
    }
  }
}

void dccthread_init(void (*func)(int), int param) {
  threads = dlist_create();

  managerThread = dccthread_create("manager", &managerFunction, 1);
  dlist_pop_right(threads);

  dccthread_create("main", func, param);

  dccthread_yield();
}

dccthread_t * dccthread_create(const char *name, void (*func)(int ), int param) {
  
  dccthread_t *newThread = malloc(sizeof(dccthread_t));
  if(!newThread) {
    printf("Failed to create a new thread.\n");
    exit(1);
  }

  getcontext(&newThread->context);
  newThread->func = func;
  newThread->param = param;
  // uc_link aponta para o contexto do gerente, indica qual contexto deve ser ativado quando a thread terminar a execução
  newThread->context.uc_link = &managerThread->context;
  // aloca uma pilha para a nova thread armazenar seus registros
  newThread->context.uc_stack.ss_sp = malloc(THREAD_STACK_SIZE);
  // define o tamanho da pilha do contexto da thread
  newThread->context.uc_stack.ss_size = THREAD_STACK_SIZE;

  makecontext(&newThread->context, (void (*)(void))func, 1, param);

  strncpy(newThread->name, name, sizeof(newThread->name));

  dlist_push_right(threads, newThread);

  return newThread;
}

void dccthread_yield(void) {
  if(currentThread) {
    getcontext(&currentThread->context);
    dlist_push_right(threads, currentThread);
    swapcontext(&currentThread->context, &managerThread->context);
  } else {
    setcontext(&managerThread->context);
  }
};

dccthread_t * dccthread_self(void) {
  return currentThread;
}

const char * dccthread_name(dccthread_t *tid) {
  if(tid) {
    return tid->name;
  } 
  return NULL;
}