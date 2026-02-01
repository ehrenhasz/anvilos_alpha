 

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "py/runtime.h"
#include "py/mpthread.h"
#include "py/gc.h"

#if MICROPY_PY_THREAD

#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <semaphore.h>

#include "shared/runtime/gchelper.h"



#ifdef SIGRTMIN
#define MP_THREAD_GC_SIGNAL (SIGRTMIN + 5)
#else
#define MP_THREAD_GC_SIGNAL (SIGUSR1)
#endif


#define THREAD_STACK_OVERFLOW_MARGIN (8192)


typedef struct _mp_thread_t {
    pthread_t id;           
    int ready;              
    void *arg;              
    struct _mp_thread_t *next;
} mp_thread_t;

static pthread_key_t tls_key;




static pthread_mutex_t thread_mutex;
static mp_thread_t *thread;



#if defined(__APPLE__)
static char thread_signal_done_name[25];
static sem_t *thread_signal_done_p;
#else
static sem_t thread_signal_done;
#endif

void mp_thread_unix_begin_atomic_section(void) {
    pthread_mutex_lock(&thread_mutex);
}

void mp_thread_unix_end_atomic_section(void) {
    pthread_mutex_unlock(&thread_mutex);
}


static void mp_thread_gc(int signo, siginfo_t *info, void *context) {
    (void)info; 
    (void)context; 
    if (signo == MP_THREAD_GC_SIGNAL) {
        gc_helper_collect_regs_and_stack();
        
        
        
        
        #if MICROPY_ENABLE_PYSTACK
        void **ptrs = (void **)(void *)MP_STATE_THREAD(pystack_start);
        gc_collect_root(ptrs, (MP_STATE_THREAD(pystack_cur) - MP_STATE_THREAD(pystack_start)) / sizeof(void *));
        #endif
        #if defined(__APPLE__)
        sem_post(thread_signal_done_p);
        #else
        sem_post(&thread_signal_done);
        #endif
    }
}

void mp_thread_init(void) {
    pthread_key_create(&tls_key, NULL);
    pthread_setspecific(tls_key, &mp_state_ctx.thread);

    
    
    pthread_mutexattr_t thread_mutex_attr;
    pthread_mutexattr_init(&thread_mutex_attr);
    pthread_mutexattr_settype(&thread_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&thread_mutex, &thread_mutex_attr);

    
    thread = malloc(sizeof(mp_thread_t));
    thread->id = pthread_self();
    thread->ready = 1;
    thread->arg = NULL;
    thread->next = NULL;

    #if defined(__APPLE__)
    snprintf(thread_signal_done_name, sizeof(thread_signal_done_name), "micropython_sem_%ld", (long)thread->id);
    thread_signal_done_p = sem_open(thread_signal_done_name, O_CREAT | O_EXCL, 0666, 0);
    #else
    sem_init(&thread_signal_done, 0, 0);
    #endif

    
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = mp_thread_gc;
    sigemptyset(&sa.sa_mask);
    sigaction(MP_THREAD_GC_SIGNAL, &sa, NULL);
}

void mp_thread_deinit(void) {
    mp_thread_unix_begin_atomic_section();
    while (thread->next != NULL) {
        mp_thread_t *th = thread;
        thread = thread->next;
        pthread_cancel(th->id);
        free(th);
    }
    mp_thread_unix_end_atomic_section();
    #if defined(__APPLE__)
    sem_close(thread_signal_done_p);
    sem_unlink(thread_signal_done_name);
    #endif
    assert(thread->id == pthread_self());
    free(thread);
}







void mp_thread_gc_others(void) {
    mp_thread_unix_begin_atomic_section();
    for (mp_thread_t *th = thread; th != NULL; th = th->next) {
        gc_collect_root(&th->arg, 1);
        if (th->id == pthread_self()) {
            continue;
        }
        if (!th->ready) {
            continue;
        }
        pthread_kill(th->id, MP_THREAD_GC_SIGNAL);
        #if defined(__APPLE__)
        sem_wait(thread_signal_done_p);
        #else
        sem_wait(&thread_signal_done);
        #endif
    }
    mp_thread_unix_end_atomic_section();
}

mp_state_thread_t *mp_thread_get_state(void) {
    return (mp_state_thread_t *)pthread_getspecific(tls_key);
}

void mp_thread_set_state(mp_state_thread_t *state) {
    pthread_setspecific(tls_key, state);
}

mp_uint_t mp_thread_get_id(void) {
    return (mp_uint_t)pthread_self();
}

void mp_thread_start(void) {
    
    #if defined(__APPLE__)
    if (mp_thread_is_realtime_enabled) {
        mp_thread_set_realtime();
    }
    #endif

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    mp_thread_unix_begin_atomic_section();
    for (mp_thread_t *th = thread; th != NULL; th = th->next) {
        if (th->id == pthread_self()) {
            th->ready = 1;
            break;
        }
    }
    mp_thread_unix_end_atomic_section();
}

mp_uint_t mp_thread_create(void *(*entry)(void *), void *arg, size_t *stack_size) {
    
    if (*stack_size == 0) {
        *stack_size = 8192 * sizeof(void *);
    }

    
    if (*stack_size < PTHREAD_STACK_MIN) {
        *stack_size = PTHREAD_STACK_MIN;
    }

    
    if (*stack_size < 2 * THREAD_STACK_OVERFLOW_MARGIN) {
        *stack_size = 2 * THREAD_STACK_OVERFLOW_MARGIN;
    }

    
    pthread_attr_t attr;
    int ret = pthread_attr_init(&attr);
    if (ret != 0) {
        goto er;
    }
    ret = pthread_attr_setstacksize(&attr, *stack_size);
    if (ret != 0) {
        goto er;
    }

    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
        goto er;
    }

    mp_thread_unix_begin_atomic_section();

    
    pthread_t id;
    ret = pthread_create(&id, &attr, entry, arg);
    if (ret != 0) {
        mp_thread_unix_end_atomic_section();
        goto er;
    }

    
    *stack_size -= THREAD_STACK_OVERFLOW_MARGIN;

    
    mp_thread_t *th = malloc(sizeof(mp_thread_t));
    th->id = id;
    th->ready = 0;
    th->arg = arg;
    th->next = thread;
    thread = th;

    mp_thread_unix_end_atomic_section();

    MP_STATIC_ASSERT(sizeof(mp_uint_t) >= sizeof(pthread_t));
    return (mp_uint_t)id;

er:
    mp_raise_OSError(ret);
}

void mp_thread_finish(void) {
    mp_thread_unix_begin_atomic_section();
    mp_thread_t *prev = NULL;
    for (mp_thread_t *th = thread; th != NULL; th = th->next) {
        if (th->id == pthread_self()) {
            if (prev == NULL) {
                thread = th->next;
            } else {
                prev->next = th->next;
            }
            free(th);
            break;
        }
        prev = th;
    }
    mp_thread_unix_end_atomic_section();
}

void mp_thread_mutex_init(mp_thread_mutex_t *mutex) {
    pthread_mutex_init(mutex, NULL);
}

int mp_thread_mutex_lock(mp_thread_mutex_t *mutex, int wait) {
    int ret;
    if (wait) {
        ret = pthread_mutex_lock(mutex);
        if (ret == 0) {
            return 1;
        }
    } else {
        ret = pthread_mutex_trylock(mutex);
        if (ret == 0) {
            return 1;
        } else if (ret == EBUSY) {
            return 0;
        }
    }
    return -ret;
}

void mp_thread_mutex_unlock(mp_thread_mutex_t *mutex) {
    pthread_mutex_unlock(mutex);
    
}

#endif 



#if defined(__APPLE__)
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>

bool mp_thread_is_realtime_enabled;


void mp_thread_set_realtime(void) {
    mach_timebase_info_data_t timebase_info;

    mach_timebase_info(&timebase_info);

    const uint64_t NANOS_PER_MSEC = 1000000ULL;
    double clock2abs = ((double)timebase_info.denom / (double)timebase_info.numer) * NANOS_PER_MSEC;

    thread_time_constraint_policy_data_t policy;
    policy.period = 0;
    policy.computation = (uint32_t)(5 * clock2abs); 
    policy.constraint = (uint32_t)(10 * clock2abs);
    policy.preemptible = FALSE;

    int kr = thread_policy_set(pthread_mach_thread_np(pthread_self()),
        THREAD_TIME_CONSTRAINT_POLICY,
        (thread_policy_t)&policy,
        THREAD_TIME_CONSTRAINT_POLICY_COUNT);

    if (kr != KERN_SUCCESS) {
        mach_error("thread_policy_set:", kr);
    }
}
#endif
