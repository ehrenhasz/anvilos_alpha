
 

#include "protocol.h"
#include "orangefs-kernel.h"
#include "orangefs-debugfs.h"
#include "orangefs-sysfs.h"

 
#ifndef ORANGEFS_VERSION
#define ORANGEFS_VERSION "upstream"
#endif

 

struct orangefs_stats orangefs_stats;

 
int hash_table_size = 509;

static ulong module_parm_debug_mask;
__u64 orangefs_gossip_debug_mask;
int op_timeout_secs = ORANGEFS_DEFAULT_OP_TIMEOUT_SECS;
int slot_timeout_secs = ORANGEFS_DEFAULT_SLOT_TIMEOUT_SECS;
int orangefs_cache_timeout_msecs = 500;
int orangefs_dcache_timeout_msecs = 50;
int orangefs_getattr_timeout_msecs = 50;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ORANGEFS Development Team");
MODULE_DESCRIPTION("The Linux Kernel VFS interface to ORANGEFS");
MODULE_PARM_DESC(module_parm_debug_mask, "debugging level (see orangefs-debug.h for values)");
MODULE_PARM_DESC(op_timeout_secs, "Operation timeout in seconds");
MODULE_PARM_DESC(slot_timeout_secs, "Slot timeout in seconds");
MODULE_PARM_DESC(hash_table_size,
		 "size of hash table for operations in progress");

static struct file_system_type orangefs_fs_type = {
	.name = "pvfs2",
	.mount = orangefs_mount,
	.kill_sb = orangefs_kill_sb,
	.owner = THIS_MODULE,
};

module_param(hash_table_size, int, 0);
module_param(module_parm_debug_mask, ulong, 0644);
module_param(op_timeout_secs, int, 0);
module_param(slot_timeout_secs, int, 0);

 
DEFINE_MUTEX(orangefs_request_mutex);

 
struct list_head *orangefs_htable_ops_in_progress;
DEFINE_SPINLOCK(orangefs_htable_ops_in_progress_lock);

 
LIST_HEAD(orangefs_request_list);

 
DEFINE_SPINLOCK(orangefs_request_list_lock);

 
DECLARE_WAIT_QUEUE_HEAD(orangefs_request_list_waitq);

static int __init orangefs_init(void)
{
	int ret;
	__u32 i = 0;

	if (op_timeout_secs < 0)
		op_timeout_secs = 0;

	if (slot_timeout_secs < 0)
		slot_timeout_secs = 0;

	 
	ret = op_cache_initialize();
	if (ret < 0)
		goto out;

	ret = orangefs_inode_cache_initialize();
	if (ret < 0)
		goto cleanup_op;

	orangefs_htable_ops_in_progress =
	    kcalloc(hash_table_size, sizeof(struct list_head), GFP_KERNEL);
	if (!orangefs_htable_ops_in_progress) {
		ret = -ENOMEM;
		goto cleanup_inode;
	}

	 
	for (i = 0; i < hash_table_size; i++)
		INIT_LIST_HEAD(&orangefs_htable_ops_in_progress[i]);

	ret = fsid_key_table_initialize();
	if (ret < 0)
		goto cleanup_progress_table;

	 
	ret = orangefs_prepare_debugfs_help_string(1);
	if (ret)
		goto cleanup_key_table;

	orangefs_debugfs_init(module_parm_debug_mask);

	ret = orangefs_sysfs_init();
	if (ret)
		goto sysfs_init_failed;

	 
	ret = orangefs_dev_init();
	if (ret < 0) {
		gossip_err("%s: could not initialize device subsystem %d!\n",
			   __func__,
			   ret);
		goto cleanup_sysfs;
	}

	ret = register_filesystem(&orangefs_fs_type);
	if (ret == 0) {
		pr_info("%s: module version %s loaded\n",
			__func__,
			ORANGEFS_VERSION);
		goto out;
	}

	orangefs_dev_cleanup();

cleanup_sysfs:
	orangefs_sysfs_exit();

sysfs_init_failed:
	orangefs_debugfs_cleanup();

cleanup_key_table:
	fsid_key_table_finalize();

cleanup_progress_table:
	kfree(orangefs_htable_ops_in_progress);

cleanup_inode:
	orangefs_inode_cache_finalize();

cleanup_op:
	op_cache_finalize();

out:
	return ret;
}

static void __exit orangefs_exit(void)
{
	int i = 0;
	gossip_debug(GOSSIP_INIT_DEBUG, "orangefs: orangefs_exit called\n");

	unregister_filesystem(&orangefs_fs_type);
	orangefs_debugfs_cleanup();
	orangefs_sysfs_exit();
	fsid_key_table_finalize();
	orangefs_dev_cleanup();
	BUG_ON(!list_empty(&orangefs_request_list));
	for (i = 0; i < hash_table_size; i++)
		BUG_ON(!list_empty(&orangefs_htable_ops_in_progress[i]));

	orangefs_inode_cache_finalize();
	op_cache_finalize();

	kfree(orangefs_htable_ops_in_progress);

	pr_info("orangefs: module version %s unloaded\n", ORANGEFS_VERSION);
}

 
void purge_inprogress_ops(void)
{
	int i;

	for (i = 0; i < hash_table_size; i++) {
		struct orangefs_kernel_op_s *op;
		struct orangefs_kernel_op_s *next;

		spin_lock(&orangefs_htable_ops_in_progress_lock);
		list_for_each_entry_safe(op,
					 next,
					 &orangefs_htable_ops_in_progress[i],
					 list) {
			set_op_state_purged(op);
			gossip_debug(GOSSIP_DEV_DEBUG,
				     "%s: op:%s: op_state:%d: process:%s:\n",
				     __func__,
				     get_opname_string(op),
				     op->op_state,
				     current->comm);
		}
		spin_unlock(&orangefs_htable_ops_in_progress_lock);
	}
}

module_init(orangefs_init);
module_exit(orangefs_exit);
