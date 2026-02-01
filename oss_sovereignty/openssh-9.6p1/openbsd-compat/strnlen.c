 

 

 

#include "includes.h"
#if !defined(HAVE_STRNLEN) || defined(BROKEN_STRNLEN)
#include <sys/types.h>

#include <string.h>

size_t
strnlen(const char *str, size_t maxlen)
{
	const char *cp;

	for (cp = str; maxlen != 0 && *cp != '\0'; cp++, maxlen--)
		;

	return (size_t)(cp - str);
}
#endif
