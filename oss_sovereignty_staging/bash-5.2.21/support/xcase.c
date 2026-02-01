 

 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "bashansi.h"
#include <errno.h>

#ifndef errno
extern int errno;
#endif

extern int optind;

#define LOWER	1
#define UPPER	2

int
main(ac, av)
int	ac;
char	**av;
{
	int	c, x;
	int	op;
	FILE	*inf;

	op = 0;
	while ((c = getopt(ac, av, "lnu")) != EOF) {
		switch (c) {
		case 'n':
			setbuf (stdout, (char *)NULL);
			break;
		case 'u':
			op = UPPER;
			break;
		case 'l':
			op = LOWER;
			break;
		default:
			fprintf(stderr, "casemod: usage: casemod [-lnu] [file]\n");
			exit(2);
		}
	}
	av += optind;
	ac -= optind;

	if (av[0] && (av[0][0] != '-' || av[0][1])) {
		inf = fopen(av[0], "r");
		if (inf == 0) {
			fprintf(stderr, "casemod: %s: cannot open: %s\n", av[0], strerror(errno));
			exit(1);
		}
	} else
		inf = stdin;

	while ((c = getc(inf)) != EOF) {
		switch (op) {
		case UPPER:
			x = islower(c) ? toupper(c) : c;
			break;
		case LOWER:
			x = isupper(c) ? tolower(c) : c;
			break;
		default:
			x = c;
			break;
		}
		putchar(x);
	}
			
	exit(0);
}
