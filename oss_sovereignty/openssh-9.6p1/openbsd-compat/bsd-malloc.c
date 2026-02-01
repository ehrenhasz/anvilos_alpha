 

#include "config.h"
#undef malloc
#undef calloc
#undef realloc

#include <sys/types.h>
#include <stdlib.h>

#if defined(HAVE_MALLOC) && HAVE_MALLOC == 0
void *
rpl_malloc(size_t size)
{
	if (size == 0)
		size = 1;
	return malloc(size);
}
#endif

#if defined(HAVE_CALLOC) && HAVE_CALLOC == 0
void *
rpl_calloc(size_t nmemb, size_t size)
{
	if (nmemb == 0)
		nmemb = 1;
	if (size == 0)
		size = 1;
	return calloc(nmemb, size);
}
#endif

#if defined (HAVE_REALLOC) && HAVE_REALLOC == 0
void *
rpl_realloc(void *ptr, size_t size)
{
	if (size == 0)
		size = 1;
	if (ptr == 0)
		return malloc(size);
	return realloc(ptr, size);
}
#endif
