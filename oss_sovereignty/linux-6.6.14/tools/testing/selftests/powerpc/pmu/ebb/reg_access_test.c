
 

#include <stdio.h>
#include <stdlib.h>

#include "ebb.h"


 
int reg_access(void)
{
	uint64_t val, expected;

	SKIP_IF(!ebb_is_supported());

	expected = 0x8000000100000000ull;
	mtspr(SPRN_BESCR, expected);
	val = mfspr(SPRN_BESCR);

	FAIL_IF(val != expected);

	expected = 0x0000000001000000ull;
	mtspr(SPRN_EBBHR, expected);
	val = mfspr(SPRN_EBBHR);

	FAIL_IF(val != expected);

	return 0;
}

int main(void)
{
	return test_harness(reg_access, "reg_access");
}
