
 

#include <stdio.h>
#include <string.h>

#include "utils.h"

static int test_denormal_fpu(void)
{
	unsigned int m32;
	unsigned long m64;
	volatile float f;
	volatile double d;

	 

	m32 = 0x00715fcf;  
	memcpy((float *)&f, &m32, sizeof(f));
	d = f;
	memcpy(&m64, (double *)&d, sizeof(d));

	FAIL_IF((long)(m64 != 0x380c57f3c0000000));  

	return 0;
}

int main(int argc, char *argv[])
{
	return test_harness(test_denormal_fpu, "fpu_denormal");
}
