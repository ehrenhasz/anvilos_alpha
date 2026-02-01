 
 

#include "libuutil_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *
uu_zalloc(size_t n)
{
	void *p = malloc(n);

	if (p == NULL) {
		uu_set_error(UU_ERROR_SYSTEM);
		return (NULL);
	}

	(void) memset(p, 0, n);

	return (p);
}

void
uu_free(void *p)
{
	free(p);
}

char *
uu_strdup(const char *str)
{
	char *buf = NULL;

	if (str != NULL) {
		size_t sz;

		sz = strlen(str) + 1;
		buf = uu_zalloc(sz);
		if (buf != NULL)
			(void) memcpy(buf, str, sz);
	}
	return (buf);
}

 
char *
uu_strndup(const char *s, size_t n)
{
	size_t len;
	char *p;

	len = strnlen(s, n);
	p = uu_zalloc(len + 1);
	if (p == NULL)
		return (NULL);

	if (len > 0)
		(void) memcpy(p, s, len);
	p[len] = '\0';

	return (p);
}

 
void *
uu_memdup(const void *buf, size_t sz)
{
	void *p;

	p = uu_zalloc(sz);
	if (p == NULL)
		return (NULL);
	(void) memcpy(p, buf, sz);
	return (p);
}

char *
uu_msprintf(const char *format, ...)
{
	va_list args;
	char attic[1];
	uint_t M, m;
	char *b;

	va_start(args, format);
	M = vsnprintf(attic, 1, format, args);
	va_end(args);

	for (;;) {
		m = M;
		if ((b = uu_zalloc(m + 1)) == NULL)
			return (NULL);

		va_start(args, format);
		M = vsnprintf(b, m + 1, format, args);
		va_end(args);

		if (M == m)
			break;		 

		uu_free(b);
	}

	return (b);
}
