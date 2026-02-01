 

 

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static __attribute__((noreturn)) void
usage(void)
{
	(void) fprintf(stderr,
	    "usage: zgenhostid [-fh] [-o path] [value]\n\n"
	    "  -f\t\t force hostid file write\n"
	    "  -h\t\t print this usage and exit\n"
	    "  -o <filename>\t write hostid to this file\n\n"
	    "If hostid file is not present, store a hostid in it.\n"
	    "The optional value should be an 8-digit hex number between"
	    " 1 and 2^32-1.\n"
	    "If the value is 0 or no value is provided, a random one"
	    " will be generated.\n"
	    "The value must be unique among your systems.\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	 
	const char *path = "/etc/hostid";
	 
	unsigned long input_i = 0;

	int opt;
	int force_fwrite = 0;
	while ((opt = getopt_long(argc, argv, "fo:h?", 0, 0)) != -1) {
		switch (opt) {
		case 'f':
			force_fwrite = 1;
			break;
		case 'o':
			path = optarg;
			break;
		case 'h':
		case '?':
			usage();
		}
	}

	char *in_s = argv[optind];
	if (in_s != NULL) {
		 
		if (strncasecmp("0x", in_s, 2) == 0) {
			in_s += 2;
		}

		 
		const char *hex = "0123456789abcdefABCDEF";
		if (strlen(in_s) != 8 || strspn(in_s, hex) != 8) {
			fprintf(stderr, "%s\n", strerror(ERANGE));
			usage();
		}

		input_i = strtoul(in_s, NULL, 16);
		if (errno != 0) {
			perror("strtoul");
			exit(EXIT_FAILURE);
		}

		if (input_i > UINT32_MAX) {
			fprintf(stderr, "%s\n", strerror(ERANGE));
			usage();
		}
	}

	struct stat fstat;
	if (force_fwrite == 0 && stat(path, &fstat) == 0 &&
	    S_ISREG(fstat.st_mode)) {
		fprintf(stderr, "%s: %s\n", path, strerror(EEXIST));
		exit(EXIT_FAILURE);
	}

	 
	while (input_i == 0) {
		srand48(getpid() ^ time(NULL));
		input_i = lrand48();
	}

	FILE *fp = fopen(path, "wb");
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	 
	uint32_t hostid = input_i;
	int written = fwrite(&hostid, 1, 4, fp);
	if (written != 4) {
		perror("fwrite");
		exit(EXIT_FAILURE);
	}

	fclose(fp);
	exit(EXIT_SUCCESS);
}
