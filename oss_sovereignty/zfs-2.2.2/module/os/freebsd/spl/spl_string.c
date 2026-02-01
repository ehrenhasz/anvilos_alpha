 
 

#include <sys/types.h>
#include <sys/param.h>
#include <sys/string.h>
#include <sys/kmem.h>
#include <machine/stdarg.h>

#define	IS_DIGIT(c)	((c) >= '0' && (c) <= '9')

#define	IS_ALPHA(c)	\
	(((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

char *
strpbrk(const char *s, const char *b)
{
	const char *p;

	do {
		for (p = b; *p != '\0' && *p != *s; ++p)
			;
		if (*p != '\0')
			return ((char *)s);
	} while (*s++);

	return (NULL);
}

 
void
strident_canon(char *s, size_t n)
{
	char c;
	char *end = s + n - 1;

	if ((c = *s) == 0)
		return;

	if (!IS_ALPHA(c) && c != '_')
		*s = '_';

	while (s < end && ((c = *(++s)) != 0)) {
		if (!IS_ALPHA(c) && !IS_DIGIT(c) && c != '_')
			*s = '_';
	}
	*s = 0;
}

 
char *
kmem_asprintf(const char *fmt, ...)
{
	int size;
	va_list adx;
	char *buf;

	va_start(adx, fmt);
	size = vsnprintf(NULL, 0, fmt, adx) + 1;
	va_end(adx);

	buf = kmem_alloc(size, KM_SLEEP);

	va_start(adx, fmt);
	(void) vsnprintf(buf, size, fmt, adx);
	va_end(adx);

	return (buf);
}

void
kmem_strfree(char *str)
{
	ASSERT3P(str, !=, NULL);
	kmem_free(str, strlen(str) + 1);
}

 

int
kmem_scnprintf(char *restrict str, size_t size, const char *restrict fmt, ...)
{
	int n;
	va_list ap;

	 
	if (size == 0)
		return (0);

	va_start(ap, fmt);
	n = vsnprintf(str, size, fmt, ap);
	va_end(ap);

	if (n >= size)
		n = size - 1;

	return (n);
}
