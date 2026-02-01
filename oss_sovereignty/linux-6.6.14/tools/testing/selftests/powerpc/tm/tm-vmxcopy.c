
 

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>

#include "tm.h"
#include "utils.h"

int test_vmxcopy()
{
	long double vecin = 1.3;
	long double vecout;
	unsigned long pgsize = getpagesize();
	int i;
	int fd;
	int size = pgsize*16;
	char tmpfile[] = "/tmp/page_faultXXXXXX";
	char buf[pgsize];
	char *a;
	uint64_t aborted = 0;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());
	SKIP_IF(!is_ppc64le());

	fd = mkstemp(tmpfile);
	assert(fd >= 0);

	memset(buf, 0, pgsize);
	for (i = 0; i < size; i += pgsize)
		assert(write(fd, buf, pgsize) == pgsize);

	unlink(tmpfile);

	a = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	assert(a != MAP_FAILED);

	asm __volatile__(
		"lxvd2x 40,0,%[vecinptr];"	 
		"tbegin.;"
		"beq	3f;"
		"tsuspend.;"
		"xxlxor 40,40,40;"		 
		"std	5, 0(%[map]);"		 
		"tabort. 0;"
		"tresume.;"
		"tend.;"
		"li	%[res], 0;"
		"b	5f;"

		 
		"3:;"
		"li	%[res], 1;"

		"5:;"
		"stxvd2x 40,0,%[vecoutptr];"
		: [res]"=&r"(aborted)
		: [vecinptr]"r"(&vecin),
		  [vecoutptr]"r"(&vecout),
		  [map]"r"(a)
		: "memory", "r0", "r3", "r4", "r5", "r6", "r7");

	if (aborted && (vecin != vecout)){
		printf("FAILED: vector state leaked on abort %f != %f\n",
		       (double)vecin, (double)vecout);
		return 1;
	}

	munmap(a, size);

	close(fd);

	return 0;
}

int main(void)
{
	return test_harness(test_vmxcopy, "tm_vmxcopy");
}
