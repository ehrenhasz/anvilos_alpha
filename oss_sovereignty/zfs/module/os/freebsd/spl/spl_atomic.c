#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/atomic.h>
#if !defined(__LP64__) && !defined(__mips_n32) && \
	!defined(ARM_HAVE_ATOMIC64) && !defined(I386_HAVE_ATOMIC64) && \
	!defined(HAS_EMULATED_ATOMIC64)
#ifdef _KERNEL
#include <sys/kernel.h>
struct mtx atomic_mtx;
MTX_SYSINIT(atomic, &atomic_mtx, "atomic", MTX_DEF);
#else
#include <pthread.h>
#define	mtx_lock(lock)		pthread_mutex_lock(lock)
#define	mtx_unlock(lock)	pthread_mutex_unlock(lock)
static pthread_mutex_t atomic_mtx;
static __attribute__((constructor)) void
atomic_init(void)
{
	pthread_mutex_init(&atomic_mtx, NULL);
}
#endif
void
atomic_add_64(volatile uint64_t *target, int64_t delta)
{
	mtx_lock(&atomic_mtx);
	*target += delta;
	mtx_unlock(&atomic_mtx);
}
void
atomic_dec_64(volatile uint64_t *target)
{
	mtx_lock(&atomic_mtx);
	*target -= 1;
	mtx_unlock(&atomic_mtx);
}
uint64_t
atomic_swap_64(volatile uint64_t *a, uint64_t value)
{
	uint64_t ret;
	mtx_lock(&atomic_mtx);
	ret = *a;
	*a = value;
	mtx_unlock(&atomic_mtx);
	return (ret);
}
uint64_t
atomic_load_64(volatile uint64_t *a)
{
	uint64_t ret;
	mtx_lock(&atomic_mtx);
	ret = *a;
	mtx_unlock(&atomic_mtx);
	return (ret);
}
uint64_t
atomic_add_64_nv(volatile uint64_t *target, int64_t delta)
{
	uint64_t newval;
	mtx_lock(&atomic_mtx);
	newval = (*target += delta);
	mtx_unlock(&atomic_mtx);
	return (newval);
}
uint64_t
atomic_cas_64(volatile uint64_t *target, uint64_t cmp, uint64_t newval)
{
	uint64_t oldval;
	mtx_lock(&atomic_mtx);
	oldval = *target;
	if (oldval == cmp)
		*target = newval;
	mtx_unlock(&atomic_mtx);
	return (oldval);
}
#endif
