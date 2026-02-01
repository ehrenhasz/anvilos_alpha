 

#include "includes.h"

#include <stdlib.h>
#include <string.h>

#ifndef HAVE_FREEZERO

void
freezero(void *ptr, size_t sz)
{
	if (ptr == NULL)
		return;
	explicit_bzero(ptr, sz);
	free(ptr);
}

#endif  

