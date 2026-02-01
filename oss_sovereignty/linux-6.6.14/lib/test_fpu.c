
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <asm/fpu/api.h>

static int test_fpu(void)
{
	 
	volatile double a, b, c, d, e, f, g;

	a = 4.0;
	b = 1e-15;
	c = 1e-310;

	 
	d = a + b;

	 
	e = a + b / 2;

	 
	f = b / c;

	 
	g = a + c * f;

	if (d > a && e > a && g > a)
		return 0;
	else
		return -EINVAL;
}

static int test_fpu_get(void *data, u64 *val)
{
	int status = -EINVAL;

	kernel_fpu_begin();
	status = test_fpu();
	kernel_fpu_end();

	*val = 1;
	return status;
}

DEFINE_DEBUGFS_ATTRIBUTE(test_fpu_fops, test_fpu_get, NULL, "%lld\n");
static struct dentry *selftest_dir;

static int __init test_fpu_init(void)
{
	selftest_dir = debugfs_create_dir("selftest_helpers", NULL);
	if (!selftest_dir)
		return -ENOMEM;

	debugfs_create_file_unsafe("test_fpu", 0444, selftest_dir, NULL,
				   &test_fpu_fops);

	return 0;
}

static void __exit test_fpu_exit(void)
{
	debugfs_remove(selftest_dir);
}

module_init(test_fpu_init);
module_exit(test_fpu_exit);

MODULE_LICENSE("GPL");
