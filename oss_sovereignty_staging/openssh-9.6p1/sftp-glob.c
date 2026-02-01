 
 

#include "includes.h"

#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "xmalloc.h"
#include "sftp.h"
#include "sftp-common.h"
#include "sftp-client.h"

int sftp_glob(struct sftp_conn *, const char *, int,
    int (*)(const char *, int), glob_t *);

struct SFTP_OPENDIR {
	SFTP_DIRENT **dir;
	int offset;
};

static struct {
	struct sftp_conn *conn;
} cur;

static void *
fudge_opendir(const char *path)
{
	struct SFTP_OPENDIR *r;

	r = xcalloc(1, sizeof(*r));

	if (sftp_readdir(cur.conn, path, &r->dir)) {
		free(r);
		return(NULL);
	}

	r->offset = 0;

	return((void *)r);
}

static struct dirent *
fudge_readdir(struct SFTP_OPENDIR *od)
{
	 
	static char buf[sizeof(struct dirent) + MAXPATHLEN];
	struct dirent *ret = (struct dirent *)buf;
#ifdef __GNU_LIBRARY__
	static int inum = 1;
#endif  

	if (od->dir[od->offset] == NULL)
		return(NULL);

	memset(buf, 0, sizeof(buf));

	 
#ifdef BROKEN_ONE_BYTE_DIRENT_D_NAME
	strlcpy(ret->d_name, od->dir[od->offset++]->filename, MAXPATHLEN);
#else
	strlcpy(ret->d_name, od->dir[od->offset++]->filename,
	    sizeof(ret->d_name));
#endif
#ifdef __GNU_LIBRARY__
	 
	ret->d_ino = inum++;
	if (!inum)
		inum = 1;
#endif  

	return(ret);
}

static void
fudge_closedir(struct SFTP_OPENDIR *od)
{
	sftp_free_dirents(od->dir);
	free(od);
}

static int
fudge_lstat(const char *path, struct stat *st)
{
	Attrib a;

	if (sftp_lstat(cur.conn, path, 1, &a) != 0)
		return -1;

	attrib_to_stat(&a, st);

	return 0;
}

static int
fudge_stat(const char *path, struct stat *st)
{
	Attrib a;

	if (sftp_stat(cur.conn, path, 1, &a) != 0)
		return -1;

	attrib_to_stat(&a, st);

	return(0);
}

int
sftp_glob(struct sftp_conn *conn, const char *pattern, int flags,
    int (*errfunc)(const char *, int), glob_t *pglob)
{
	int r;
	size_t l;
	char *s;
	struct stat sb;

	pglob->gl_opendir = fudge_opendir;
	pglob->gl_readdir = (struct dirent *(*)(void *))fudge_readdir;
	pglob->gl_closedir = (void (*)(void *))fudge_closedir;
	pglob->gl_lstat = fudge_lstat;
	pglob->gl_stat = fudge_stat;

	memset(&cur, 0, sizeof(cur));
	cur.conn = conn;

	if ((r = glob(pattern, flags | GLOB_ALTDIRFUNC, errfunc, pglob)) != 0)
		return r;
	 
	if ((flags & (GLOB_NOCHECK|GLOB_MARK)) == (GLOB_NOCHECK|GLOB_MARK) &&
	    pglob->gl_matchc == 0 && pglob->gl_offs == 0 &&
	    pglob->gl_pathc == 1 && (s = pglob->gl_pathv[0]) != NULL &&
	    (l = strlen(s)) > 0 && s[l-1] != '/') {
		if (fudge_stat(s, &sb) == 0 && S_ISDIR(sb.st_mode)) {
			 
			if ((s = realloc(s, l + 2)) != NULL) {
				memcpy(s + l, "/", 2);
				pglob->gl_pathv[0] = s;
			}
		}
	}
	return 0;
}
