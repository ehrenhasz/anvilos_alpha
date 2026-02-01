
 

#define FSCACHE_DEBUG_LEVEL CACHE
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "internal.h"

 
int __init fscache_proc_init(void)
{
	if (!proc_mkdir("fs/fscache", NULL))
		goto error_dir;

	if (!proc_create_seq("fs/fscache/caches", S_IFREG | 0444, NULL,
			     &fscache_caches_seq_ops))
		goto error;

	if (!proc_create_seq("fs/fscache/volumes", S_IFREG | 0444, NULL,
			     &fscache_volumes_seq_ops))
		goto error;

	if (!proc_create_seq("fs/fscache/cookies", S_IFREG | 0444, NULL,
			     &fscache_cookies_seq_ops))
		goto error;

#ifdef CONFIG_FSCACHE_STATS
	if (!proc_create_single("fs/fscache/stats", S_IFREG | 0444, NULL,
				fscache_stats_show))
		goto error;
#endif

	return 0;

error:
	remove_proc_entry("fs/fscache", NULL);
error_dir:
	return -ENOMEM;
}

 
void fscache_proc_cleanup(void)
{
	remove_proc_subtree("fs/fscache", NULL);
}
