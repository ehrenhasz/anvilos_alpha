
 

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <asm/tm.h>

#include "utils.h"
#include "tm.h"
#include "../pmu/lib.h"

#define SPRN_DSCR       0x03

int test_body(void)
{
	uint64_t rv, dscr1 = 1, dscr2, texasr;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	printf("Check DSCR TM context switch: ");
	fflush(stdout);
	for (;;) {
		asm __volatile__ (
			 
			"ld      3, %[dscr1];"
			"mtspr   %[sprn_dscr], 3;"

			"li      %[rv], 1;"
			 
			"tbegin.;"
			"beq     1f;"
			"tsuspend.;"

			 
			"2: ;"
			"tcheck 0;"
			"bc      4, 0, 2b;"

			 
			"mfspr   3, %[sprn_dscr];"
			"std     3, %[dscr2];"
			"mfspr   3, %[sprn_texasr];"
			"std     3, %[texasr];"

			"tresume.;"
			"tend.;"
			"li      %[rv], 0;"
			"1: ;"
			: [rv]"=r"(rv), [dscr2]"=m"(dscr2), [texasr]"=m"(texasr)
			: [dscr1]"m"(dscr1)
			, [sprn_dscr]"i"(SPRN_DSCR), [sprn_texasr]"i"(SPRN_TEXASR)
			: "memory", "r3"
		);
		assert(rv);  
		if ((texasr >> 56) != TM_CAUSE_RESCHED) {
			continue;
		}
		if (dscr2 != dscr1) {
			printf(" FAIL\n");
			return 1;
		} else {
			printf(" OK\n");
			return 0;
		}
	}
}

static int tm_resched_dscr(void)
{
	return eat_cpu(test_body);
}

int main(int argc, const char *argv[])
{
	return test_harness(tm_resched_dscr, "tm_resched_dscr");
}
