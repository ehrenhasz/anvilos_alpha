



#ifndef	_SYS_WMSUM_H
#define	_SYS_WMSUM_H

#include <linux/percpu_counter.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct percpu_counter	wmsum_t;

static inline void
wmsum_init(wmsum_t *ws, uint64_t value)
{

#ifdef HAVE_PERCPU_COUNTER_INIT_WITH_GFP
	percpu_counter_init(ws, value, GFP_KERNEL);
#else
	percpu_counter_init(ws, value);
#endif
}

static inline void
wmsum_fini(wmsum_t *ws)
{

	percpu_counter_destroy(ws);
}

static inline uint64_t
wmsum_value(wmsum_t *ws)
{

	return (percpu_counter_sum(ws));
}

static inline void
wmsum_add(wmsum_t *ws, int64_t delta)
{

#ifdef HAVE_PERCPU_COUNTER_ADD_BATCH
	percpu_counter_add_batch(ws, delta, INT_MAX / 2);
#else
	__percpu_counter_add(ws, delta, INT_MAX / 2);
#endif
}

#ifdef	__cplusplus
}
#endif

#endif 
