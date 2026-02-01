 
 

 

#include "includes.h"

#if !defined(HAVE_GETCWD)

#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <sys/dir.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "includes.h"

#define	ISDOT(dp) \
	(dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || \
	    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))

char *
getcwd(char *pt, size_t size)
{
	struct dirent *dp;
	DIR *dir = NULL;
	dev_t dev;
	ino_t ino;
	int first;
	char *bpt, *bup;
	struct stat s;
	dev_t root_dev;
	ino_t root_ino;
	size_t ptsize, upsize;
	int save_errno;
	char *ept, *eup, *up;

	 
	if (pt) {
		ptsize = 0;
		if (size == 0) {
			errno = EINVAL;
			return (NULL);
		} else if (size == 1) {
			errno = ERANGE;
			return (NULL);
		}
		ept = pt + size;
	} else {
		if ((pt = malloc(ptsize = MAXPATHLEN)) == NULL)
			return (NULL);
		ept = pt + ptsize;
	}
	bpt = ept - 1;
	*bpt = '\0';

	 
	if ((up = malloc(upsize = MAXPATHLEN)) == NULL)
		goto err;
	eup = up + upsize;
	bup = up;
	up[0] = '.';
	up[1] = '\0';

	 
	if (stat("/", &s))
		goto err;
	root_dev = s.st_dev;
	root_ino = s.st_ino;

	errno = 0;			 

	for (first = 1;; first = 0) {
		 
		if (lstat(up, &s))
			goto err;

		 
		ino = s.st_ino;
		dev = s.st_dev;

		 
		if (root_dev == dev && root_ino == ino) {
			*--bpt = '/';
			 
			memmove(pt, bpt, ept - bpt);
			free(up);
			return (pt);
		}

		 
		if (bup + 3  + MAXNAMLEN + 1 >= eup) {
			char *nup;

			if ((nup = realloc(up, upsize *= 2)) == NULL)
				goto err;
			bup = nup + (bup - up);
			up = nup;
			eup = up + upsize;
		}
		*bup++ = '.';
		*bup++ = '.';
		*bup = '\0';

		 
		if (!(dir = opendir(up)) || fstat(dirfd(dir), &s))
			goto err;

		 
		*bup++ = '/';

		 
		save_errno = 0;
		if (s.st_dev == dev) {
			for (;;) {
				if (!(dp = readdir(dir)))
					goto notfound;
				if (dp->d_fileno == ino)
					break;
			}
		} else
			for (;;) {
				if (!(dp = readdir(dir)))
					goto notfound;
				if (ISDOT(dp))
					continue;
				memcpy(bup, dp->d_name, dp->d_namlen + 1);

				 
				if (lstat(up, &s)) {
					if (!save_errno)
						save_errno = errno;
					errno = 0;
					continue;
				}
				if (s.st_dev == dev && s.st_ino == ino)
					break;
			}

		 
		if (bpt - pt < dp->d_namlen + (first ? 1 : 2)) {
			size_t len;
			char *npt;

			if (!ptsize) {
				errno = ERANGE;
				goto err;
			}
			len = ept - bpt;
			if ((npt = realloc(pt, ptsize *= 2)) == NULL)
				goto err;
			bpt = npt + (bpt - pt);
			pt = npt;
			ept = pt + ptsize;
			memmove(ept - len, bpt, len);
			bpt = ept - len;
		}
		if (!first)
			*--bpt = '/';
		bpt -= dp->d_namlen;
		memcpy(bpt, dp->d_name, dp->d_namlen);
		(void)closedir(dir);

		 
		*bup = '\0';
	}

notfound:
	 
	if (!errno)
		errno = save_errno ? save_errno : ENOENT;
	 
err:
	save_errno = errno;

	if (ptsize)
		free(pt);
	free(up);
	if (dir)
		(void)closedir(dir);

	errno = save_errno;

	return (NULL);
}

#endif  
