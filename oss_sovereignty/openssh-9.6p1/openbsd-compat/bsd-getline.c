 

 

 

 

#include "includes.h"

#if 0
#include "file.h"
#endif

#if !defined(HAVE_GETLINE) || defined(BROKEN_GETLINE)
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static ssize_t
getdelim(char **buf, size_t *bufsiz, int delimiter, FILE *fp)
{
	char *ptr, *eptr;


	if (*buf == NULL || *bufsiz == 0) {
		if ((*buf = malloc(BUFSIZ)) == NULL)
			return -1;
		*bufsiz = BUFSIZ;
	}

	for (ptr = *buf, eptr = *buf + *bufsiz;;) {
		int c = fgetc(fp);
		if (c == -1) {
			if (feof(fp)) {
				ssize_t diff = (ssize_t)(ptr - *buf);
				if (diff != 0) {
					*ptr = '\0';
					return diff;
				}
			}
			return -1;
		}
		*ptr++ = c;
		if (c == delimiter) {
			*ptr = '\0';
			return ptr - *buf;
		}
		if (ptr + 2 >= eptr) {
			char *nbuf;
			size_t nbufsiz = *bufsiz * 2;
			ssize_t d = ptr - *buf;
			if ((nbuf = realloc(*buf, nbufsiz)) == NULL)
				return -1;
			*buf = nbuf;
			*bufsiz = nbufsiz;
			eptr = nbuf + nbufsiz;
			ptr = nbuf + d;
		}
	}
}

ssize_t
getline(char **buf, size_t *bufsiz, FILE *fp)
{
	return getdelim(buf, bufsiz, '\n', fp);
}

#endif

#ifdef TEST
int
main(int argc, char *argv[])
{
	char *p = NULL;
	ssize_t len;
	size_t n = 0;

	while ((len = getline(&p, &n, stdin)) != -1)
		(void)printf("%" SIZE_T_FORMAT "d %s", len, p);
	free(p);
	return 0;
}
#endif
