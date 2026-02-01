
 

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

char *target;
char *depfile;
char *cmdline;

static void usage(void)
{
	fprintf(stderr, "Usage: fixdep <depfile> <target> <cmdline>\n");
	exit(1);
}

 
static void print_cmdline(void)
{
	printf("cmd_%s := %s\n\n", target, cmdline);
}

 
static void parse_dep_file(void *map, size_t len)
{
	char *m = map;
	char *end = m + len;
	char *p;
	char s[PATH_MAX];
	int is_target, has_target = 0;
	int saw_any_target = 0;
	int is_first_dep = 0;

	while (m < end) {
		 
		while (m < end && (*m == ' ' || *m == '\\' || *m == '\n'))
			m++;
		 
		p = m;
		while (p < end && *p != ' ' && *p != '\\' && *p != '\n')
			p++;
		 
		is_target = (*(p-1) == ':');
		 
		if (is_target) {
			 
			is_first_dep = 1;
			has_target = 1;
		} else if (has_target) {
			 
			memcpy(s, m, p-m);
			s[p - m] = 0;

			 
			if (is_first_dep) {
				 
				if (!saw_any_target) {
					saw_any_target = 1;
					printf("source_%s := %s\n\n",
						target, s);
					printf("deps_%s := \\\n",
						target);
				}
				is_first_dep = 0;
			} else
				printf("  %s \\\n", s);
		}
		 
		m = p + 1;
	}

	if (!saw_any_target) {
		fprintf(stderr, "fixdep: parse error; no targets found\n");
		exit(1);
	}

	printf("\n%s: $(deps_%s)\n\n", target, target);
	printf("$(deps_%s):\n", target);
}

static void print_deps(void)
{
	struct stat st;
	int fd;
	void *map;

	fd = open(depfile, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "fixdep: error opening depfile: ");
		perror(depfile);
		exit(2);
	}
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "fixdep: error fstat'ing depfile: ");
		perror(depfile);
		exit(2);
	}
	if (st.st_size == 0) {
		fprintf(stderr, "fixdep: %s is empty\n", depfile);
		close(fd);
		return;
	}
	map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if ((long) map == -1) {
		perror("fixdep: mmap");
		close(fd);
		return;
	}

	parse_dep_file(map, st.st_size);

	munmap(map, st.st_size);

	close(fd);
}

int main(int argc, char **argv)
{
	if (argc != 4)
		usage();

	depfile = argv[1];
	target  = argv[2];
	cmdline = argv[3];

	print_cmdline();
	print_deps();

	return 0;
}
