 

#include "includes.h"

#include <stdlib.h>
#include <string.h>

static int fail = 0;

void
test(const char *a)
{
	char *b;

	b = strdup(a);
	if (b == 0) {
		fail = 1;
		return;
	}
	if (strcmp(a, b) != 0)
		fail = 1;
	free(b);
}

int
main(void)
{
	test("");
	test("a");
	test("\0");
	test("abcdefghijklmnopqrstuvwxyz");
	return fail;
}
