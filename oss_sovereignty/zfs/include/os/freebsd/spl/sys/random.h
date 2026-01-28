#ifndef _OPENSOLARIS_SYS_RANDOM_H_
#define	_OPENSOLARIS_SYS_RANDOM_H_
#include_next <sys/random.h>
#if  __FreeBSD_version >= 1300108
#include <sys/prng.h>
#endif
static inline int
random_get_bytes(uint8_t *p, size_t s)
{
	arc4rand(p, (int)s, 0);
	return (0);
}
static inline int
random_get_pseudo_bytes(uint8_t *p, size_t s)
{
	arc4rand(p, (int)s, 0);
	return (0);
}
static inline uint32_t
random_in_range(uint32_t range)
{
#if defined(_KERNEL) && __FreeBSD_version >= 1300108
	return (prng32_bounded(range));
#else
	uint32_t r;
	ASSERT(range != 0);
	if (range == 1)
		return (0);
	(void) random_get_pseudo_bytes((uint8_t *)&r, sizeof (r));
	return (r % range);
#endif
}
#endif	 
