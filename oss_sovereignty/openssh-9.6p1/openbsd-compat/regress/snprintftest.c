 

#define BUFSZ 2048

#include "includes.h"

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int failed = 0;

static void
fail(const char *m)
{
	fprintf(stderr, "snprintftest: %s\n", m);
	failed = 1;
}

int x_snprintf(char *str, size_t count, const char *fmt, ...)
{
	size_t ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vsnprintf(str, count, fmt, ap);
	va_end(ap);
	return ret;
}

int
main(void)
{
	char b[5];
	char *src = NULL;
	int ret;

	memset(b, 'X', sizeof(b));
	ret = snprintf(b, 5, "123456789");
	if (ret != 9 || b[4] != '\0')
		fail("snprintf does not correctly terminate long strings");

	 
	if ((src = malloc(BUFSZ)) == NULL) {
		fail("malloc failed");
	} else {
		memset(src, 'a', BUFSZ);
		snprintf(b, sizeof(b), "%.*s", 1, src);
		if (strcmp(b, "a") != 0)
			fail("failed with length limit '%%.s'");
	}

	 
	if (snprintf(b, 1, "%s %d", "hello", 12345) != 11)
		fail("snprintf does not return required length");
	if (x_snprintf(b, 1, "%s %d", "hello", 12345) != 11)
		fail("vsnprintf does not return required length");

	free(src);
	return failed;
}
