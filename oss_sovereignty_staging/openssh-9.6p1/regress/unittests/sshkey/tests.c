 
 

#include "includes.h"

#include "../test_helper/test_helper.h"

void sshkey_tests(void);
void sshkey_file_tests(void);
void sshkey_fuzz_tests(void);

void
tests(void)
{
	sshkey_tests();
	sshkey_file_tests();
	sshkey_fuzz_tests();
}
