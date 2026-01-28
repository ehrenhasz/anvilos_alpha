



#if defined(__linux__)

#include <pthread.h>

#define mutex_t			pthread_mutex_t
#define cond_t			pthread_cond_t
#define rwlock_t		pthread_rwlock_t
#define once_t			pthread_once_t

#define thread_key_t		pthread_key_t

#define KEY_INITIALIZER		((thread_key_t)-1)
#define MUTEX_INITIALIZER	PTHREAD_MUTEX_INITIALIZER
#define RWLOCK_INITIALIZER	PTHREAD_RWLOCK_INITIALIZER
#define ONCE_INITIALIZER	PTHREAD_ONCE_INIT

#define mutex_init(m, a)	pthread_mutex_init(m, a)
#define mutex_lock(m)		pthread_mutex_lock(m)
#define mutex_unlock(m)		pthread_mutex_unlock(m)

#define cond_init(c, a, p)	pthread_cond_init(c, a)
#define cond_destroy(c)		pthread_cond_destroy(c)
#define cond_signal(m)		pthread_cond_signal(m)
#define cond_broadcast(m)	pthread_cond_broadcast(m)
#define cond_wait(c, m)		pthread_cond_wait(c, m)

#define rwlock_init(l, a)	pthread_rwlock_init(l, a)
#define rwlock_rdlock(l)	pthread_rwlock_rdlock(l)
#define rwlock_wrlock(l)	pthread_rwlock_wrlock(l)
#define rwlock_unlock(l)	pthread_rwlock_unlock(l)

#define thr_keycreate(k, d)	pthread_key_create(k, d)
#define thr_setspecific(k, p)	pthread_setspecific(k, p)
#define thr_getspecific(k)	pthread_getspecific(k)
#define thr_sigsetmask(f, n, o)	pthread_sigmask(f, n, o)

#define thr_once(o, i)		pthread_once(o, i)
#define thr_self()		pthread_self()
#define thr_exit(x)		pthread_exit(x)

#endif
