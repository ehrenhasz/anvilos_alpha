
 

 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>

static int livepatch_callbacks_mod_init(void)
{
	pr_info("%s\n", __func__);
	return 0;
}

static void livepatch_callbacks_mod_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(livepatch_callbacks_mod_init);
module_exit(livepatch_callbacks_mod_exit);
MODULE_LICENSE("GPL");
