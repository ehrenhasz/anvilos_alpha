 

 

#ifndef	_SYS_WMSUM_H
#define	_SYS_WMSUM_H

#include <sys/aggsum.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	wmsum_t	aggsum_t

static inline void
wmsum_init(wmsum_t *ws, uint64_t value)
{

	aggsum_init(ws, value);
}

static inline void
wmsum_fini(wmsum_t *ws)
{

	aggsum_fini(ws);
}

static inline uint64_t
wmsum_value(wmsum_t *ws)
{

	return (aggsum_value(ws));
}

static inline void
wmsum_add(wmsum_t *ws, int64_t delta)
{

	aggsum_add(ws, delta);
}

#ifdef	__cplusplus
}
#endif

#endif  
