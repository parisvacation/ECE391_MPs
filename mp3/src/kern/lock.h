// lock.h - A sleep lock
//

#ifdef LOCK_TRACE
#define TRACE
#endif

#ifdef LOCK_DEBUG
#define DEBUG
#endif

#ifndef _LOCK_H_
#define _LOCK_H_

#include "thread.h"
#include "halt.h"
#include "console.h"
#include "intr.h"

struct lock {
    struct condition cond;
    int tid; // thread id holding lock or -1
};

static inline void lock_init(struct lock * lk, const char * name);
static inline void lock_acquire(struct lock * lk);
static inline void lock_release(struct lock * lk);

// INLINE FUNCTION DEFINITIONS
//

static inline void lock_init(struct lock * lk, const char * name) {
    trace("%s(<%s:%p>", __func__, name, lk);
    condition_init(&lk->cond, name);
    lk->tid = -1;
}

/**
 * @brief Acquire lock
 * 
 * @param lk The lock to be acquired
 * This function acquires the lock for the current thread.
 */
static inline void lock_acquire(struct lock * lk) {
    // TODO: FIXME implement this
    trace("%s(<%s:%p>", __func__, lk->cond.name, lk);
    
    // Get the current running thread id
    int curr_tid = running_thread();
    
    // Check if the current thread has already acquired the lock
    if(lk->tid == curr_tid){
    	debug("Thread <%s:%d> already holds lock <%s:%p>",
            thread_name(curr_tid), curr_tid,
            lk->cond.name, lk);
    	return;
	}
	
    // Disable the interrupt
    int saved_intr_state = intr_disable();

    // If the lock is acquired by other thread, sleep the current thread
    while(lk->tid != -1){
        debug("Thread <%s:%d> is waiting for lock <%s:%p>",
            thread_name(curr_tid), curr_tid,
            lk->cond.name, lk);
		condition_wait(&lk->cond);
	}
	
    // Acquire the lock
	lk->tid = curr_tid;

    // Restore the interrupt state
    intr_restore(saved_intr_state);
	
	debug("Thread <%s:%d> acquired lock <%s:%p>",
        thread_name(curr_tid), curr_tid,
        lk->cond.name, lk);
}

static inline void lock_release(struct lock * lk) {
    trace("%s(<%s:%p>", __func__, lk->cond.name, lk);

    assert (lk->tid == running_thread());
    
    // Disable the interrupt
    int saved_intr_state = intr_disable();

    lk->tid = -1;
    condition_broadcast(&lk->cond);

    // Restore the interrupt state
    intr_restore(saved_intr_state);

    debug("Thread <%s:%d> released lock <%s:%p>",
        thread_name(running_thread()), running_thread(),
        lk->cond.name, lk);
}

#endif // _LOCK_H_