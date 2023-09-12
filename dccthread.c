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

struct sigevent sev;
struct itimerspec its;
timer_t timerid;
sigset_t signal_mask;

void blockSignals() {
  sigprocmask(SIG_BLOCK, &signal_mask, NULL);
}

void unblockSignals() {
  sigprocmask(SIG_UNBLOCK, &signal_mask, NULL);
  timer_settime(timerid, 0, &its, NULL);
}

void managerFunction(int running) {
  blockSignals();
  if (running) {
    if(!dlist_empty(threads)) {
      dccthread_t *thread = dlist_pop_left(threads);
      currentThread = thread;
      setcontext(&thread->context);
    } else {
      running = 0;
    }
  }
  unblockSignals();
}

void timerHandler(int signo, siginfo_t *info, void *context) {
  dccthread_yield();
}

void setupTimer() {
  // configura o tratador de sinais para o temporizador
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = timerHandler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGALRM, &sa, NULL);

  // cria o temporizador
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGALRM;
  timer_create(CLOCK_REALTIME, &sev, &timerid);

  // configura o intervalo do temporizador (10 ms)
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = TIMER_INTERVAL_NS;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = TIMER_INTERVAL_NS;
  timer_settime(timerid, 0, &its, NULL);
}

void dccthread_init(void (*func)(int), int param) {
  while(1) {
    threads = dlist_create();
    waitingThreads = dlist_create();
    
    setupTimer();

    managerThread = dccthread_create("manager", &managerFunction, 1);
    dlist_pop_right(threads);

    dccthread_create("main", func, param);

    dccthread_yield();
  }
}

dccthread_t * dccthread_create(const char *name, void (*func)(int ), int param) {
  blockSignals();

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
  strncpy(newThread->name, name, sizeof(newThread->name));
  makecontext(&newThread->context, (void (*)(void))func, 1, param);

  dlist_push_right(threads, newThread);

  return newThread;

  unblockSignals();
}

void dccthread_yield(void) {
  blockSignals();
  if(currentThread) {
    getcontext(&currentThread->context);
    dlist_push_right(threads, currentThread);
    swapcontext(&currentThread->context, &managerThread->context);
  } else {
    setcontext(&managerThread->context);
  }
  unblockSignals();
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

  return strcmp(threadPtr->waitingFor, name) == 0;
}

void dccthread_exit(void) {
  blockSignals();
  getcontext(&currentThread->context);

  dccthread_t *waitingThread = dlist_find_remove(waitingThreads, currentThread, waitingCompare, NULL);
  if(waitingThread) {
    dlist_push_right(threads, waitingThread);
  }

  setcontext(&managerThread->context);
  unblockSignals();
}

void dccthread_wait(dccthread_t *tid) {
  blockSignals();
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
  unblockSignals();
}

struct TimerInfo {
  timer_t *timerid;
  dccthread_t *thread;
};

void wakeUpThread(int signo, siginfo_t *info, void *context) {
  struct TimerInfo *timerInfo = (struct TimerInfo *)info->si_value.sival_ptr;

  dlist_push_right(threads, timerInfo->thread);
  
  timer_delete(*(timerInfo->timerid));

  swapcontext(&currentThread->context, &managerThread->context);
}

void dccthread_sleep(struct timespec ts) {
  timer_t sleeptimerid;
  struct TimerInfo timerInfo;

  getcontext(&currentThread->context);
  timerInfo.thread = currentThread;

  // sinal do temporizador
  struct sigaction sleepsa;
  sleepsa.sa_flags = SA_SIGINFO;
  sleepsa.sa_sigaction = wakeUpThread;
  sigemptyset(&sleepsa.sa_mask);
  sigaction(SIGALRM, &sleepsa, NULL);

  // parâmetros do temporizador
  struct sigevent sleepsev;
  sleepsev.sigev_notify = SIGEV_SIGNAL;
  sleepsev.sigev_signo = SIGALRM;
  sleepsev.sigev_value.sival_ptr = &timerInfo;

  timer_create(CLOCK_REALTIME, &sleepsev, &sleeptimerid);
  timerInfo.timerid = &sleeptimerid;

  // configurar temporizador
  struct itimerspec sleepits;
  sleepits.it_value.tv_sec = ts.tv_sec;
  sleepits.it_value.tv_nsec = ts.tv_nsec;
  sleepits.it_interval.tv_sec = ts.tv_sec;
  sleepits.it_interval.tv_nsec = ts.tv_nsec;
  timer_settime(sleeptimerid, 0, &sleepits, NULL);

  swapcontext(&currentThread->context, &managerThread->context);
}