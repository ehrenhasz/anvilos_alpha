 

 

#ifndef	_THREAD_POOL_H_
#define	_THREAD_POOL_H_ extern __attribute__((visibility("default")))

#include <sys/types.h>
#include <pthread.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct tpool tpool_t;	 

_THREAD_POOL_H_ tpool_t	*tpool_create(uint_t min_threads, uint_t max_threads,
			uint_t linger, pthread_attr_t *attr);
_THREAD_POOL_H_ int	tpool_dispatch(tpool_t *tpool,
			void (*func)(void *), void *arg);
_THREAD_POOL_H_ void	tpool_destroy(tpool_t *tpool);
_THREAD_POOL_H_ void	tpool_abandon(tpool_t *tpool);
_THREAD_POOL_H_ void	tpool_wait(tpool_t *tpool);
_THREAD_POOL_H_ void	tpool_suspend(tpool_t *tpool);
_THREAD_POOL_H_ int	tpool_suspended(tpool_t *tpool);
_THREAD_POOL_H_ void	tpool_resume(tpool_t *tpool);
_THREAD_POOL_H_ int	tpool_member(tpool_t *tpool);

#ifdef	__cplusplus
}
#endif

#endif	 
