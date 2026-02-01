 

 

#ifndef	_SYS_WMSUM_H
#define	_SYS_WMSUM_H

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/malloc.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	wmsum_t	counter_u64_t

static inline void
wmsum_init(wmsum_t *ws, uint64_t value)
{

	*ws = counter_u64_alloc(M_WAITOK);
	counter_u64_add(*ws, value);
}

static inline void
wmsum_fini(wmsum_t *ws)
{

	counter_u64_free(*ws);
}

static inline uint64_t
wmsum_value(wmsum_t *ws)
{

	return (counter_u64_fetch(*ws));
}

static inline void
wmsum_add(wmsum_t *ws, int64_t delta)
{

	counter_u64_add(*ws, delta);
}

#ifdef	__cplusplus
}
#endif

#endif  
