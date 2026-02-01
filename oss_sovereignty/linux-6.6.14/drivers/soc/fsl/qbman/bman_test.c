 

#include "bman_test.h"

MODULE_AUTHOR("Geoff Thorpe");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("BMan testing");

static int test_init(void)
{
#ifdef CONFIG_FSL_BMAN_TEST_API
	int loop = 1;

	while (loop--)
		bman_test_api();
#endif
	return 0;
}

static void test_exit(void)
{
}

module_init(test_init);
module_exit(test_exit);
