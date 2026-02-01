
 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <asm/cputable.h>

#include "utils.h"

#define SIZE (64 * 1024)

int test_prot_sao(void)
{
	char *p;

	 
	SKIP_IF(!have_hwcap(PPC_FEATURE_ARCH_2_06) ||
		have_hwcap2(PPC_FEATURE2_ARCH_3_1) ||
		access("/proc/device-tree/rtas/ibm,hypertas-functions", F_OK) == 0);

	 
	p = mmap(NULL, SIZE, PROT_READ | PROT_WRITE | PROT_SAO,
		 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	FAIL_IF(p == MAP_FAILED);

	 
	memset(p, 0xaa, SIZE);

	return 0;
}

int main(void)
{
	return test_harness(test_prot_sao, "prot-sao");
}
