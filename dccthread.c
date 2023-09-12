#define _POSIX_C_SOURCE 199309L

#include "dccthread.h"
#include "dlist.h"
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define TIMER_INTERVAL_NS 10000000

struct dccthread {
  ucontext_t context;
  char name[DCCTHREAD_MAX_NAME_SIZE];
  void (*func) (int);
  int param;
  char waitingFor[DCCTHREAD_MAX_NAME_SIZE];
};

struct dlist *threads;
struct dlist *waitingThreads;
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

void timerHandler(int signum, siginfo_t *info, void *context) {
  dccthread_yield();
}

void setupTimer() {
  struct sigevent sev;
  timer_t timerid;
  struct itimerspec its;
  struct sigaction sa;

  // sinal do temporizador
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = timerHandler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGALRM, &sa, NULL);

  // parâmetros do temporizador
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGALRM;
  sev.sigev_value.sival_ptr = &timerid;
  timer_create(CLOCK_REALTIME, &sev, &timerid);

  // configurar temporizador
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = TIMER_INTERVAL_NS;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = TIMER_INTERVAL_NS;
  timer_settime(timerid, 0, &its, NULL);
}

void dccthread_init(void (*func)(int), int param) {
  threads = dlist_create();
  waitingThreads = dlist_create();

  setupTimer();

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

int waitingCompare(const void *thread, const void *threadName, void *userdata) { 
  const dccthread_t *threadPtr = (const dccthread_t *)thread;
  const char *name = (const char *)threadName;

  return (threadPtr->waitingFor == name);
}

void dccthread_exit(void) {
  getcontext(&currentThread->context);

  dccthread_t *waitingThread = dlist_find_remove(waitingThreads, currentThread, waitingCompare, NULL);
  if(waitingThread) {
    dlist_push_right(threads, waitingThread);
  }

  setcontext(&managerThread->context);
}

void dccthread_wait(dccthread_t *tid) {
  if(tid && currentThread) {
    strncpy(currentThread->waitingFor, tid->name, sizeof(tid->name));
    getcontext(&currentThread->context);

    dlist_push_right(waitingThreads, currentThread);
    dlist_push_right(threads, tid);

    swapcontext(&currentThread->context, &managerThread->context);
  } else if (tid) {
    dlist_push_right(threads, tid);
  } else {
    setcontext(&managerThread->context);
  }
}

