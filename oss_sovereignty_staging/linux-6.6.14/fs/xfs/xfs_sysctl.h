
 
#ifndef __XFS_SYSCTL_H__
#define __XFS_SYSCTL_H__

#include <linux/sysctl.h>

 

typedef struct xfs_sysctl_val {
	int min;
	int val;
	int max;
} xfs_sysctl_val_t;

typedef struct xfs_param {
	xfs_sysctl_val_t sgid_inherit;	 
	xfs_sysctl_val_t symlink_mode;	 
	xfs_sysctl_val_t panic_mask;	 
	xfs_sysctl_val_t error_level;	 
	xfs_sysctl_val_t syncd_timer;	 
	xfs_sysctl_val_t stats_clear;	 
	xfs_sysctl_val_t inherit_sync;	 
	xfs_sysctl_val_t inherit_nodump; 
	xfs_sysctl_val_t inherit_noatim; 
	xfs_sysctl_val_t xfs_buf_timer;	 
	xfs_sysctl_val_t xfs_buf_age;	 
	xfs_sysctl_val_t inherit_nosym;	 
	xfs_sysctl_val_t rotorstep;	 
	xfs_sysctl_val_t inherit_nodfrg; 
	xfs_sysctl_val_t fstrm_timer;	 
	xfs_sysctl_val_t blockgc_timer;	 
} xfs_param_t;

 

enum {
	 
	 
	 
	XFS_SGID_INHERIT = 4,
	XFS_SYMLINK_MODE = 5,
	XFS_PANIC_MASK = 6,
	XFS_ERRLEVEL = 7,
	XFS_SYNCD_TIMER = 8,
	 
	 
	 
	XFS_STATS_CLEAR = 12,
	XFS_INHERIT_SYNC = 13,
	XFS_INHERIT_NODUMP = 14,
	XFS_INHERIT_NOATIME = 15,
	XFS_BUF_TIMER = 16,
	XFS_BUF_AGE = 17,
	 
	XFS_INHERIT_NOSYM = 19,
	XFS_ROTORSTEP = 20,
	XFS_INHERIT_NODFRG = 21,
	XFS_FILESTREAM_TIMER = 22,
};

extern xfs_param_t	xfs_params;

struct xfs_globals {
#ifdef DEBUG
	int	pwork_threads;		 
	bool	larp;			 
#endif
	int	log_recovery_delay;	 
	int	mount_delay;		 
	bool	bug_on_assert;		 
	bool	always_cow;		 
};
extern struct xfs_globals	xfs_globals;

#ifdef CONFIG_SYSCTL
extern int xfs_sysctl_register(void);
extern void xfs_sysctl_unregister(void);
#else
# define xfs_sysctl_register()		(0)
# define xfs_sysctl_unregister()	do { } while (0)
#endif  

#endif  
