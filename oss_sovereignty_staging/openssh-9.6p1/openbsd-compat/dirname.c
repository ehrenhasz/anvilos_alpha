 

 

 

#include "includes.h"
#ifndef HAVE_DIRNAME

#include <errno.h>
#include <string.h>

char *
dirname(const char *path)
{
	static char dname[MAXPATHLEN];
	size_t len;
	const char *endp;

	 
	if (path == NULL || *path == '\0') {
		dname[0] = '.';
		dname[1] = '\0';
		return (dname);
	}

	 
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/')
		endp--;

	 
	while (endp > path && *endp != '/')
		endp--;

	 
	if (endp == path) {
		dname[0] = *endp == '/' ? '/' : '.';
		dname[1] = '\0';
		return (dname);
	} else {
		 
		do {
			endp--;
		} while (endp > path && *endp == '/');
	}

	len = endp - path + 1;
	if (len >= sizeof(dname)) {
		errno = ENAMETOOLONG;
		return (NULL);
	}
	memcpy(dname, path, len);
	dname[len] = '\0';
	return (dname);
}
#endif
