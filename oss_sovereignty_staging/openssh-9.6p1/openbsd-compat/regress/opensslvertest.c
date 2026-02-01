 

#include "includes.h"

#include <stdio.h>
#include <stdlib.h>

int ssh_compatible_openssl(long, long);

struct version_test {
	long headerver;
	long libver;
	int result;
} version_tests[] = {
	 
	{ 0x1000101fL, 0x1000101fL, 1}, 
	{ 0x1000101fL, 0x1000102fL, 1},	 
	{ 0x1000101fL, 0x1000100fL, 1},	 
	{ 0x1000101fL, 0x1000201fL, 1},	 
	{ 0x1000101fL, 0x1000001fL, 0},	 
	{ 0x1000101fL, 0x1010101fL, 0},	 
	{ 0x1000101fL, 0x0000101fL, 0},	 
	{ 0x1000101fL, 0x2000101fL, 0},	 

	 
	{ 0x1010101fL, 0x1010101fL, 1}, 
	{ 0x1010101fL, 0x1010102fL, 1},	 
	{ 0x1010101fL, 0x1010100fL, 1},	 
	{ 0x1010101fL, 0x1010201fL, 1},	 
	{ 0x1010101fL, 0x1010001fL, 0},	 
	{ 0x1010101fL, 0x1020001fL, 0},	 
	{ 0x1010101fL, 0x0010101fL, 0},	 
	{ 0x1010101fL, 0x2010101fL, 0},	 

	 
	{ 0x3010101fL, 0x3010101fL, 1}, 
	{ 0x3010101fL, 0x3010102fL, 1},	 
	{ 0x3010101fL, 0x3010100fL, 1},	 
	{ 0x3010101fL, 0x3010201fL, 1},	 
	{ 0x3010101fL, 0x3010001fL, 1},	 
	{ 0x3010101fL, 0x3020001fL, 1},	 
	{ 0x3010101fL, 0x1010101fL, 0},	 
	{ 0x3010101fL, 0x4010101fL, 0},	 
};

void
fail(long hver, long lver, int result)
{
	fprintf(stderr, "opensslver: header %lx library %lx != %d \n", hver, lver, result);
	exit(1);
}

int
main(void)
{
#ifdef WITH_OPENSSL
	unsigned int i;
	int res;
	long hver, lver;

	for (i = 0; i < sizeof(version_tests) / sizeof(version_tests[0]); i++) {
		hver = version_tests[i].headerver;
		lver = version_tests[i].libver;
		res = version_tests[i].result;
		if (ssh_compatible_openssl(hver, lver) != res)
			fail(hver, lver, res);
	}
#endif
	exit(0);
}
