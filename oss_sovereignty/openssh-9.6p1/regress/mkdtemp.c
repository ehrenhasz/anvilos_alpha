 

 

#include "includes.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

static void
usage(void)
{
	fprintf(stderr, "mkdtemp template\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	const char *base;
	const char *tmpdir;
	char template[PATH_MAX];
	int r;
	char *dir;

	if (argc != 2)
		usage();
	base = argv[1];

	if ((tmpdir = getenv("TMPDIR")) == NULL)
		tmpdir = "/tmp";
	r = snprintf(template, sizeof(template), "%s/%s", tmpdir, base);
	if (r < 0 || (size_t)r >= sizeof(template))
		fatal("template string too long");
	dir = mkdtemp(template);
	if (dir == NULL) {
		perror("mkdtemp");
		exit(1);
	}
	puts(dir);
	return 0;
}
