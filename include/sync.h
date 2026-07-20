#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include "wait.h"

// Mutex: ownership + wait queue. Recursive locking NOT supported.
typedef struct {
    wait_queue_t wq;
    int          owner;   // task id, -1 = unlocked
    uint32_t     locked;
} mutex_t;

#define MUTEX_INIT { WAIT_QUEUE_INIT, -1, 0 }

void mutex_init(mutex_t* m);
void mutex_lock(mutex_t* m);
int  mutex_trylock(mutex_t* m);   // 1 = acquired, 0 = busy
void mutex_unlock(mutex_t* m);

// Counting semaphore.
typedef struct {
    wait_queue_t wq;
    int32_t      count;
} semaphore_t;

#define SEMAPHORE_INIT(n) { WAIT_QUEUE_INIT, (n) }

void semaphore_init(semaphore_t* s, int32_t initial);
void semaphore_wait(semaphore_t* s);    // P / down
int  semaphore_trywait(semaphore_t* s); // 1 = acquired, 0 = would block
void semaphore_post(semaphore_t* s);    // V / up

// Condition variable. Always used with a mutex held by the waiter.
typedef struct {
    wait_queue_t wq;
} condvar_t;

#define CONDVAR_INIT { WAIT_QUEUE_INIT }

void condvar_init(condvar_t* c);
void condvar_wait(condvar_t* c, mutex_t* m);   // atomically release m + block, re-lock on wake
void condvar_signal(condvar_t* c);
void condvar_broadcast(condvar_t* c);

#endif // SYNC_H
