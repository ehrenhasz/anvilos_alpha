
 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "tm.h"
#include "utils.h"

int	num_loops	= 10000;

int test_tar(void)
{
	int i;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());
	SKIP_IF(!is_ppc64le());

	for (i = 0; i < num_loops; i++)
	{
		uint64_t result = 0;
		asm __volatile__(
			"li	7, 1;"
			"mtspr	%[tar], 7;"	 
			"tbegin.;"
			"beq	3f;"
			"li	4, 0x7000;"	 
			"2:;"			 
			"li	7, 2;"
			"mtspr	%[tar], 7;"	 
			"tsuspend.;"
			"li	7, 3;"
			"mtspr	%[tar], 7;"	 
			"tresume.;"
			"subi	4, 4, 1;"
			"cmpdi	4, 0;"
			"bne	2b;"
			"tend.;"

			 
			"mfspr  7, %[tar];"
			"ori	%[res], 7, 4;"  
			"b	4f;"

			 
			"3:;"
			"mfspr  7, %[tar];"
			"ori	%[res], 7, 8;"	
			"4:;"

			: [res]"=r"(result)
			: [tar]"i"(SPRN_TAR)
			   : "memory", "r0", "r4", "r7");

		 
		if ((result != 7) && (result != 9))
			return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	 
	if (argc > 1) {
		if (strcmp(argv[1], "-h") == 0) {
			printf("Syntax:\n\t%s [<num loops>]\n",
			       argv[0]);
			return 1;
		} else {
			num_loops = atoi(argv[1]);
		}
	}

	printf("Starting, %d loops\n", num_loops);

	return test_harness(test_tar, "tm_tar");
}
