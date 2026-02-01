 

 

#include "includes.h"

#include <sys/types.h>
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static void
usage(void)
{
	fprintf(stderr, "check-setuid [path]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	const char *path = ".";
	struct statvfs sb;

	if (argc > 2)
		usage();
	else if (argc == 2)
		path = argv[1];

	if (statvfs(path, &sb) != 0) {
		 
		if (errno == ENOSYS)
			return 0;
		fprintf(stderr, "statvfs for \"%s\" failed: %s\n",
		     path, strerror(errno));
	}
	return (sb.f_flag & ST_NOSUID) ? 1 : 0;
}


