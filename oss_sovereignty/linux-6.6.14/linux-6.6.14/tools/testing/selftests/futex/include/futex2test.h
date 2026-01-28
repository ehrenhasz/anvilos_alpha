#include <stdint.h>
#define u64_to_ptr(x) ((void *)(uintptr_t)(x))
static inline int futex_waitv(volatile struct futex_waitv *waiters, unsigned long nr_waiters,
			      unsigned long flags, struct timespec *timo, clockid_t clockid)
{
	return syscall(__NR_futex_waitv, waiters, nr_waiters, flags, timo, clockid);
}
