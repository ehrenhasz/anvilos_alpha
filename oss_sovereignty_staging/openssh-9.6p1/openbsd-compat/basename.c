 

 

 

#include "includes.h"
#ifndef HAVE_BASENAME
#include <errno.h>
#include <string.h>

char *
basename(const char *path)
{
	static char bname[MAXPATHLEN];
	size_t len;
	const char *endp, *startp;

	 
	if (path == NULL || *path == '\0') {
		bname[0] = '.';
		bname[1] = '\0';
		return (bname);
	}

	 
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/')
		endp--;

	 
	if (endp == path && *endp == '/') {
		bname[0] = '/';
		bname[1] = '\0';
		return (bname);
	}

	 
	startp = endp;
	while (startp > path && *(startp - 1) != '/')
		startp--;

	len = endp - startp + 1;
	if (len >= sizeof(bname)) {
		errno = ENAMETOOLONG;
		return (NULL);
	}
	memcpy(bname, startp, len);
	bname[len] = '\0';
	return (bname);
}

#endif  
