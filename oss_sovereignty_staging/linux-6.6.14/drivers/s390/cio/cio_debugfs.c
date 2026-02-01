
 

#include <linux/debugfs.h>
#include "cio_debug.h"

struct dentry *cio_debugfs_dir;

 
static int __init cio_debugfs_init(void)
{
	cio_debugfs_dir = debugfs_create_dir("cio", arch_debugfs_dir);

	return 0;
}
subsys_initcall(cio_debugfs_init);
