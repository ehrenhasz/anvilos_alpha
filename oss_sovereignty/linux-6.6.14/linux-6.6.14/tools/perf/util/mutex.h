#ifndef __PERF_MUTEX_H
#define __PERF_MUTEX_H
#include <pthread.h>
#include <stdbool.h>
#ifdef __has_attribute
#define HAVE_ATTRIBUTE(x) __has_attribute(x)
#else
#define HAVE_ATTRIBUTE(x) 0
#endif
#if HAVE_ATTRIBUTE(guarded_by) && HAVE_ATTRIBUTE(pt_guarded_by) && \
	HAVE_ATTRIBUTE(lockable) && HAVE_ATTRIBUTE(exclusive_lock_function) && \
	HAVE_ATTRIBUTE(exclusive_trylock_function) && HAVE_ATTRIBUTE(exclusive_locks_required) && \
	HAVE_ATTRIBUTE(no_thread_safety_analysis)
#define GUARDED_BY(x) __attribute__((guarded_by(x)))
#define PT_GUARDED_BY(x) __attribute__((pt_guarded_by(x)))
#define LOCKABLE __attribute__((lockable))
#define EXCLUSIVE_LOCK_FUNCTION(...)  __attribute__((exclusive_lock_function(__VA_ARGS__)))
#define UNLOCK_FUNCTION(...) __attribute__((unlock_function(__VA_ARGS__)))
#define EXCLUSIVE_TRYLOCK_FUNCTION(...) \
	__attribute__((exclusive_trylock_function(__VA_ARGS__)))
#define EXCLUSIVE_LOCKS_REQUIRED(...) __attribute__((exclusive_locks_required(__VA_ARGS__)))
#define NO_THREAD_SAFETY_ANALYSIS __attribute__((no_thread_safety_analysis))
#else
#define GUARDED_BY(x)
#define PT_GUARDED_BY(x)
#define LOCKABLE
#define EXCLUSIVE_LOCK_FUNCTION(...)
#define UNLOCK_FUNCTION(...)
#define EXCLUSIVE_TRYLOCK_FUNCTION(...)
#define EXCLUSIVE_LOCKS_REQUIRED(...)
#define NO_THREAD_SAFETY_ANALYSIS
#endif
struct LOCKABLE mutex {
	pthread_mutex_t lock;
};
struct cond {
	pthread_cond_t cond;
};
void mutex_init(struct mutex *mtx);
void mutex_init_pshared(struct mutex *mtx);
void mutex_destroy(struct mutex *mtx);
void mutex_lock(struct mutex *mtx) EXCLUSIVE_LOCK_FUNCTION(*mtx);
void mutex_unlock(struct mutex *mtx) UNLOCK_FUNCTION(*mtx);
bool mutex_trylock(struct mutex *mtx) EXCLUSIVE_TRYLOCK_FUNCTION(true, *mtx);
void cond_init(struct cond *cnd);
void cond_init_pshared(struct cond *cnd);
void cond_destroy(struct cond *cnd);
void cond_wait(struct cond *cnd, struct mutex *mtx) EXCLUSIVE_LOCKS_REQUIRED(mtx);
void cond_signal(struct cond *cnd);
void cond_broadcast(struct cond *cnd);
#endif  
