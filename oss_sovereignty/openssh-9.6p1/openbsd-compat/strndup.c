 

 

#include "includes.h"
#if !defined(HAVE_STRNDUP) || defined(BROKEN_STRNDUP)
#include <sys/types.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char *
strndup(const char *str, size_t maxlen)
{
	char *copy;
	size_t len;

	len = strnlen(str, maxlen);
	copy = malloc(len + 1);
	if (copy != NULL) {
		(void)memcpy(copy, str, len);
		copy[len] = '\0';
	}

	return copy;
}
DEF_WEAK(strndup);
#endif   
