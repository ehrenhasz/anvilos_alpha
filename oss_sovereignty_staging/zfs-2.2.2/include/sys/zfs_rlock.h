 
 
 

#ifndef	_SYS_FS_ZFS_RLOCK_H
#define	_SYS_FS_ZFS_RLOCK_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/avl.h>

typedef enum {
	RL_READER,
	RL_WRITER,
	RL_APPEND
} zfs_rangelock_type_t;

struct zfs_locked_range;

typedef void (zfs_rangelock_cb_t)(struct zfs_locked_range *, void *);

typedef struct zfs_rangelock {
	avl_tree_t rl_tree;  
	kmutex_t rl_lock;
	zfs_rangelock_cb_t *rl_cb;
	void *rl_arg;
} zfs_rangelock_t;

typedef struct zfs_locked_range {
	zfs_rangelock_t *lr_rangelock;  
	avl_node_t lr_node;	 
	uint64_t lr_offset;	 
	uint64_t lr_length;	 
	uint_t lr_count;	 
	zfs_rangelock_type_t lr_type;  
	kcondvar_t lr_write_cv;	 
	kcondvar_t lr_read_cv;	 
	uint8_t lr_proxy;	 
	uint8_t lr_write_wanted;  
	uint8_t lr_read_wanted;	 
} zfs_locked_range_t;

void zfs_rangelock_init(zfs_rangelock_t *, zfs_rangelock_cb_t *, void *);
void zfs_rangelock_fini(zfs_rangelock_t *);

zfs_locked_range_t *zfs_rangelock_enter(zfs_rangelock_t *,
    uint64_t, uint64_t, zfs_rangelock_type_t);
zfs_locked_range_t *zfs_rangelock_tryenter(zfs_rangelock_t *,
    uint64_t, uint64_t, zfs_rangelock_type_t);
void zfs_rangelock_exit(zfs_locked_range_t *);
void zfs_rangelock_reduce(zfs_locked_range_t *, uint64_t, uint64_t);

#ifdef	__cplusplus
}
#endif

#endif	 
