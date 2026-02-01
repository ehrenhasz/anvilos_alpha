 
 

#include "../test_helper/test_helper.h"

void kex_tests(void);
void kex_proposal_tests(void);
void kex_proposal_populate_tests(void);

void
tests(void)
{
	kex_tests();
	kex_proposal_tests();
	kex_proposal_populate_tests();
}
