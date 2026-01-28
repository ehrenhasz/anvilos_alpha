#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mnttab.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <unistd.h>
int
getextmntent(const char *path, struct extmnttab *entry, struct stat64 *statbuf)
{
	struct statfs sfs;
	if (strlen(path) >= MAXPATHLEN) {
		(void) fprintf(stderr, "invalid object; pathname too long\n");
		return (-1);
	}
	if (stat64(path, statbuf) != 0) {
		(void) fprintf(stderr, "cannot open '%s': %s\n",
		    path, strerror(errno));
		return (-1);
	}
	if (statfs(path, &sfs) != 0) {
		(void) fprintf(stderr, "%s: %s\n", path,
		    strerror(errno));
		return (-1);
	}
	statfs2mnttab(&sfs, (struct mnttab *)entry);
	return (0);
}
