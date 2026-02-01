
 

#ifdef DEBUG

#include <linux/module.h>

#ifdef CONFIG_SYSCTL

#include <linux/proc_fs.h>
#include <linux/sysctl.h>

#include "sysctl.h"
#include "debug.h"

 
static struct ctl_table ntfs_sysctls[] = {
	{
		.procname	= "ntfs-debug",
		.data		= &debug_msgs,		 
		.maxlen		= sizeof(debug_msgs),
		.mode		= 0644,			 
		.proc_handler	= proc_dointvec
	},
	{}
};

 
static struct ctl_table_header *sysctls_root_table;

 
int ntfs_sysctl(int add)
{
	if (add) {
		BUG_ON(sysctls_root_table);
		sysctls_root_table = register_sysctl("fs", ntfs_sysctls);
		if (!sysctls_root_table)
			return -ENOMEM;
	} else {
		BUG_ON(!sysctls_root_table);
		unregister_sysctl_table(sysctls_root_table);
		sysctls_root_table = NULL;
	}
	return 0;
}

#endif  
#endif  
