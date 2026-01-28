#ifndef _SYS_STACK_H
#define	_SYS_STACK_H
#include <pthread.h>
#define	STACK_BIAS	0
#ifdef __USE_GNU
static inline int
stack_getbounds(stack_t *sp)
{
	pthread_attr_t attr;
	int rc;
	rc = pthread_getattr_np(pthread_self(), &attr);
	if (rc)
		return (rc);
	rc = pthread_attr_getstack(&attr, &sp->ss_sp, &sp->ss_size);
	if (rc == 0)
		sp->ss_flags = 0;
	pthread_attr_destroy(&attr);
	return (rc);
}
static inline int
thr_stksegment(stack_t *sp)
{
	int rc;
	rc = stack_getbounds(sp);
	if (rc)
		return (rc);
	sp->ss_sp = (void *)(((uintptr_t)sp->ss_sp) + sp->ss_size);
	sp->ss_flags = 0;
	return (rc);
}
#endif  
#endif  
