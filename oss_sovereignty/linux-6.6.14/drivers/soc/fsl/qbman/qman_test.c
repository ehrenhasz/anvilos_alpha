 

#include "qman_test.h"

MODULE_AUTHOR("Geoff Thorpe");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("QMan testing");

static int test_init(void)
{
	int loop = 1;
	int err = 0;

	while (loop--) {
#ifdef CONFIG_FSL_QMAN_TEST_STASH
		err = qman_test_stash();
		if (err)
			break;
#endif
#ifdef CONFIG_FSL_QMAN_TEST_API
		err = qman_test_api();
		if (err)
			break;
#endif
	}
	return err;
}

static void test_exit(void)
{
}

module_init(test_init);
module_exit(test_exit);
