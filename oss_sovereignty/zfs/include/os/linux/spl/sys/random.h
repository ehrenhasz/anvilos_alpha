#ifndef _SPL_RANDOM_H
#define	_SPL_RANDOM_H
#include <linux/module.h>
#include <linux/random.h>
static __inline__ int
random_get_bytes(uint8_t *ptr, size_t len)
{
	get_random_bytes((void *)ptr, (int)len);
	return (0);
}
extern int random_get_pseudo_bytes(uint8_t *ptr, size_t len);
static __inline__ uint32_t
random_in_range(uint32_t range)
{
	uint32_t r;
	ASSERT(range != 0);
	if (range == 1)
		return (0);
	(void) random_get_pseudo_bytes((uint8_t *)&r, sizeof (r));
	return (r % range);
}
#endif	 
