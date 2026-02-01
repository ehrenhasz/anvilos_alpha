 
 

 
 

 

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#ifdef mkstemp
#undef mkstemp
#endif
int mkstemp(char *);

 
int
_ssh_mkstemp(char *template)
{
	mode_t mask;
	int ret;

	mask = umask(0177);
	ret = mkstemp(template);
	(void)umask(mask);
	return ret;
}

#if !defined(HAVE_MKDTEMP)

#define MKTEMP_NAME	0
#define MKTEMP_FILE	1
#define MKTEMP_DIR	2

#define TEMPCHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define NUM_CHARS	(sizeof(TEMPCHARS) - 1)

static int
mktemp_internal(char *path, int slen, int mode)
{
	char *start, *cp, *ep;
	const char *tempchars = TEMPCHARS;
	unsigned int r, tries;
	struct stat sb;
	size_t len;
	int fd;

	len = strlen(path);
	if (len == 0 || slen < 0 || (size_t)slen >= len) {
		errno = EINVAL;
		return(-1);
	}
	ep = path + len - slen;

	tries = 1;
	for (start = ep; start > path && start[-1] == 'X'; start--) {
		if (tries < INT_MAX / NUM_CHARS)
			tries *= NUM_CHARS;
	}
	tries *= 2;

	do {
		for (cp = start; cp != ep; cp++) {
			r = arc4random_uniform(NUM_CHARS);
			*cp = tempchars[r];
		}

		switch (mode) {
		case MKTEMP_NAME:
			if (lstat(path, &sb) != 0)
				return(errno == ENOENT ? 0 : -1);
			break;
		case MKTEMP_FILE:
			fd = open(path, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);
			if (fd != -1 || errno != EEXIST)
				return(fd);
			break;
		case MKTEMP_DIR:
			if (mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR) == 0)
				return(0);
			if (errno != EEXIST)
				return(-1);
			break;
		}
	} while (--tries);

	errno = EEXIST;
	return(-1);
}

#if 0
char *_mktemp(char *);

char *
_mktemp(char *path)
{
	if (mktemp_internal(path, 0, MKTEMP_NAME) == -1)
		return(NULL);
	return(path);
}

__warn_references(mktemp,
    "warning: mktemp() possibly used unsafely; consider using mkstemp()");

char *
mktemp(char *path)
{
	return(_mktemp(path));
}
#endif

int
mkstemp(char *path)
{
	return(mktemp_internal(path, 0, MKTEMP_FILE));
}

int
mkstemps(char *path, int slen)
{
	return(mktemp_internal(path, slen, MKTEMP_FILE));
}

char *
mkdtemp(char *path)
{
	int error;

	error = mktemp_internal(path, 0, MKTEMP_DIR);
	return(error ? NULL : path);
}

#endif  
