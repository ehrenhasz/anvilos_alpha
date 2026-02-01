 
 

#ifndef _FUTEXTEST_H
#define _FUTEXTEST_H

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/futex.h>

typedef volatile u_int32_t futex_t;
#define FUTEX_INITIALIZER 0

 
#ifndef FUTEX_WAIT_BITSET
#define FUTEX_WAIT_BITSET		9
#endif
#ifndef FUTEX_WAKE_BITSET
#define FUTEX_WAKE_BITSET		10
#endif
#ifndef FUTEX_WAIT_REQUEUE_PI
#define FUTEX_WAIT_REQUEUE_PI		11
#endif
#ifndef FUTEX_CMP_REQUEUE_PI
#define FUTEX_CMP_REQUEUE_PI		12
#endif
#ifndef FUTEX_WAIT_REQUEUE_PI_PRIVATE
#define FUTEX_WAIT_REQUEUE_PI_PRIVATE	(FUTEX_WAIT_REQUEUE_PI | \
					 FUTEX_PRIVATE_FLAG)
#endif
#ifndef FUTEX_REQUEUE_PI_PRIVATE
#define FUTEX_CMP_REQUEUE_PI_PRIVATE	(FUTEX_CMP_REQUEUE_PI | \
					 FUTEX_PRIVATE_FLAG)
#endif

 
#define futex(uaddr, op, val, timeout, uaddr2, val3, opflags) \
	syscall(SYS_futex, uaddr, op | opflags, val, timeout, uaddr2, val3)

 
static inline int
futex_wait(futex_t *uaddr, futex_t val, struct timespec *timeout, int opflags)
{
	return futex(uaddr, FUTEX_WAIT, val, timeout, NULL, 0, opflags);
}

 
static inline int
futex_wake(futex_t *uaddr, int nr_wake, int opflags)
{
	return futex(uaddr, FUTEX_WAKE, nr_wake, NULL, NULL, 0, opflags);
}

 
static inline int
futex_wait_bitset(futex_t *uaddr, futex_t val, struct timespec *timeout,
		  u_int32_t bitset, int opflags)
{
	return futex(uaddr, FUTEX_WAIT_BITSET, val, timeout, NULL, bitset,
		     opflags);
}

 
static inline int
futex_wake_bitset(futex_t *uaddr, int nr_wake, u_int32_t bitset, int opflags)
{
	return futex(uaddr, FUTEX_WAKE_BITSET, nr_wake, NULL, NULL, bitset,
		     opflags);
}

 
static inline int
futex_lock_pi(futex_t *uaddr, struct timespec *timeout, int detect,
	      int opflags)
{
	return futex(uaddr, FUTEX_LOCK_PI, detect, timeout, NULL, 0, opflags);
}

 
static inline int
futex_unlock_pi(futex_t *uaddr, int opflags)
{
	return futex(uaddr, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0, opflags);
}

 
static inline int
futex_wake_op(futex_t *uaddr, futex_t *uaddr2, int nr_wake, int nr_wake2,
	      int wake_op, int opflags)
{
	return futex(uaddr, FUTEX_WAKE_OP, nr_wake, nr_wake2, uaddr2, wake_op,
		     opflags);
}

 
static inline int
futex_requeue(futex_t *uaddr, futex_t *uaddr2, int nr_wake, int nr_requeue,
	      int opflags)
{
	return futex(uaddr, FUTEX_REQUEUE, nr_wake, nr_requeue, uaddr2, 0,
		     opflags);
}

 
static inline int
futex_cmp_requeue(futex_t *uaddr, futex_t val, futex_t *uaddr2, int nr_wake,
		  int nr_requeue, int opflags)
{
	return futex(uaddr, FUTEX_CMP_REQUEUE, nr_wake, nr_requeue, uaddr2,
		     val, opflags);
}

 
static inline int
futex_wait_requeue_pi(futex_t *uaddr, futex_t val, futex_t *uaddr2,
		      struct timespec *timeout, int opflags)
{
	return futex(uaddr, FUTEX_WAIT_REQUEUE_PI, val, timeout, uaddr2, 0,
		     opflags);
}

 
static inline int
futex_cmp_requeue_pi(futex_t *uaddr, futex_t val, futex_t *uaddr2, int nr_wake,
		     int nr_requeue, int opflags)
{
	return futex(uaddr, FUTEX_CMP_REQUEUE_PI, nr_wake, nr_requeue, uaddr2,
		     val, opflags);
}

 
static inline u_int32_t
futex_cmpxchg(futex_t *uaddr, u_int32_t oldval, u_int32_t newval)
{
	return __sync_val_compare_and_swap(uaddr, oldval, newval);
}

 
static inline u_int32_t
futex_dec(futex_t *uaddr)
{
	return __sync_sub_and_fetch(uaddr, 1);
}

 
static inline u_int32_t
futex_inc(futex_t *uaddr)
{
	return __sync_add_and_fetch(uaddr, 1);
}

 
static inline u_int32_t
futex_set(futex_t *uaddr, u_int32_t newval)
{
	*uaddr = newval;
	return newval;
}

#endif
