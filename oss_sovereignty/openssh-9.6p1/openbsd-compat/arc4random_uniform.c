 

 

 

#include "includes.h"

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdlib.h>

#ifndef HAVE_ARC4RANDOM_UNIFORM
 
uint32_t
arc4random_uniform(uint32_t upper_bound)
{
	uint32_t r, min;

	if (upper_bound < 2)
		return 0;

	 
	min = -upper_bound % upper_bound;

	 
	for (;;) {
		r = arc4random();
		if (r >= min)
			break;
	}

	return r % upper_bound;
}
#endif  
