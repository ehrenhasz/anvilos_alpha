


#ifndef _DEBUGFS_INTERNAL_H_
#define _DEBUGFS_INTERNAL_H_

struct file_operations;


extern const struct file_operations debugfs_noop_file_operations;
extern const struct file_operations debugfs_open_proxy_file_operations;
extern const struct file_operations debugfs_full_proxy_file_operations;

struct debugfs_fsdata {
	const struct file_operations *real_fops;
	union {
		
		debugfs_automount_t automount;
		struct {
			refcount_t active_users;
			struct completion active_users_drained;
		};
	};
};


#define DEBUGFS_FSDATA_IS_REAL_FOPS_BIT BIT(0)


#define DEBUGFS_ALLOW_API	BIT(0)
#define DEBUGFS_ALLOW_MOUNT	BIT(1)

#ifdef CONFIG_DEBUG_FS_ALLOW_ALL
#define DEFAULT_DEBUGFS_ALLOW_BITS (DEBUGFS_ALLOW_MOUNT | DEBUGFS_ALLOW_API)
#endif
#ifdef CONFIG_DEBUG_FS_DISALLOW_MOUNT
#define DEFAULT_DEBUGFS_ALLOW_BITS (DEBUGFS_ALLOW_API)
#endif
#ifdef CONFIG_DEBUG_FS_ALLOW_NONE
#define DEFAULT_DEBUGFS_ALLOW_BITS (0)
#endif

#endif 
