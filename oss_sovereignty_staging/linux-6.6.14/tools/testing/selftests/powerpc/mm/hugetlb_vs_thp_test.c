
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "utils.h"

 
#define SIZE	(16 * 1024 * 1024)

static int test_body(void)
{
	void *addr;
	char *p;

	addr = (void *)0xa0000000;

	p = mmap(addr, SIZE, PROT_READ | PROT_WRITE,
		 MAP_HUGETLB | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (p != MAP_FAILED) {
		 
		if (munmap(addr, SIZE)) {
			perror("munmap");
			return 1;
		}
	}

	p = mmap(addr, SIZE, PROT_READ | PROT_WRITE,
		 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (p == MAP_FAILED) {
		printf("Mapping failed @ %p\n", addr);
		perror("mmap");
		return 1;
	}

	 
	*p = 0xf;

	munmap(addr, SIZE);

	return 0;
}

static int test_main(void)
{
	int i;

	 
	for (i = 0; i < 10000; i++)
		if (test_body())
			return 1;

	return 0;
}

int main(void)
{
	return test_harness(test_main, "hugetlb_vs_thp");
}
