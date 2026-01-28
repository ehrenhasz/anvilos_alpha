

#ifndef	_SYS_ZVOL_IMPL_H
#define	_SYS_ZVOL_IMPL_H

#include <sys/zfs_context.h>

#define	ZVOL_RDONLY	0x1

#define	ZVOL_WRITTEN_TO	0x2

#define	ZVOL_DUMPIFIED	0x4

#define	ZVOL_EXCL	0x8


typedef struct zvol_state {
	char			zv_name[MAXNAMELEN];	
	uint64_t		zv_volsize;		
	uint64_t		zv_volblocksize;	
	objset_t		*zv_objset;	
	uint32_t		zv_flags;	
	uint32_t		zv_open_count;	
	uint32_t		zv_changed;	
	uint32_t		zv_volmode;	
	zilog_t			*zv_zilog;	
	zfs_rangelock_t		zv_rangelock;	
	dnode_t			*zv_dn;		
	dataset_kstats_t	zv_kstat;	
	list_node_t		zv_next;	
	uint64_t		zv_hash;	
	struct hlist_node	zv_hlink;	
	kmutex_t		zv_state_lock;	
	atomic_t		zv_suspend_ref;	
	krwlock_t		zv_suspend_lock;	
	struct zvol_state_os	*zv_zso;	
} zvol_state_t;


extern krwlock_t zvol_state_lock;
#define	ZVOL_HT_SIZE	1024
extern struct hlist_head *zvol_htable;
#define	ZVOL_HT_HEAD(hash)	(&zvol_htable[(hash) & (ZVOL_HT_SIZE-1)])
extern zil_replay_func_t *const zvol_replay_vector[TX_MAX_TYPE];

extern unsigned int zvol_volmode;
extern unsigned int zvol_inhibit_dev;


zvol_state_t *zvol_find_by_name_hash(const char *name,
    uint64_t hash, int mode);
int zvol_first_open(zvol_state_t *zv, boolean_t readonly);
uint64_t zvol_name_hash(const char *name);
void zvol_remove_minors_impl(const char *name);
void zvol_last_close(zvol_state_t *zv);
void zvol_insert(zvol_state_t *zv);
void zvol_log_truncate(zvol_state_t *zv, dmu_tx_t *tx, uint64_t off,
    uint64_t len, boolean_t sync);
void zvol_log_write(zvol_state_t *zv, dmu_tx_t *tx, uint64_t offset,
    uint64_t size, int sync);
int zvol_get_data(void *arg, uint64_t arg2, lr_write_t *lr, char *buf,
    struct lwb *lwb, zio_t *zio);
int zvol_init_impl(void);
void zvol_fini_impl(void);
void zvol_wait_close(zvol_state_t *zv);


void zvol_os_free(zvol_state_t *zv);
void zvol_os_rename_minor(zvol_state_t *zv, const char *newname);
int zvol_os_create_minor(const char *name);
int zvol_os_update_volsize(zvol_state_t *zv, uint64_t volsize);
boolean_t zvol_os_is_zvol(const char *path);
void zvol_os_clear_private(zvol_state_t *zv);
void zvol_os_set_disk_ro(zvol_state_t *zv, int flags);
void zvol_os_set_capacity(zvol_state_t *zv, uint64_t capacity);

#endif
