#ifndef _ZFS_PERCPU_H
#define	_ZFS_PERCPU_H
#include <linux/percpu_counter.h>
#ifdef HAVE_PERCPU_COUNTER_INIT_WITH_GFP
#define	percpu_counter_init_common(counter, n, gfp) \
	percpu_counter_init(counter, n, gfp)
#else
#define	percpu_counter_init_common(counter, n, gfp) \
	percpu_counter_init(counter, n)
#endif
#endif  
