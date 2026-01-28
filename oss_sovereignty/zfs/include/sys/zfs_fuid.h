#ifndef	_SYS_FS_ZFS_FUID_H
#define	_SYS_FS_ZFS_FUID_H
#ifdef _KERNEL
#include <sys/sid.h>
#include <sys/dmu.h>
#include <sys/zfs_vfsops.h>
#endif
#include <sys/avl.h>
#ifdef	__cplusplus
extern "C" {
#endif
typedef enum {
	ZFS_OWNER,
	ZFS_GROUP,
	ZFS_ACE_USER,
	ZFS_ACE_GROUP
} zfs_fuid_type_t;
#define	FUID_SIZE_ESTIMATE(z) ((z)->z_fuid_size + (SPA_MINBLOCKSIZE << 1))
#define	FUID_INDEX(x)	((x) >> 32)
#define	FUID_RID(x)	((x) & 0xffffffff)
#define	FUID_ENCODE(idx, rid) (((uint64_t)(idx) << 32) | (rid))
typedef struct zfs_fuid {
	list_node_t 	z_next;
	uint64_t 	z_id;		 
	uint64_t	z_domidx;	 
	uint64_t	z_logfuid;	 
} zfs_fuid_t;
typedef struct zfs_fuid_domain {
	list_node_t	z_next;
	uint64_t	z_domidx;	 
	const char	*z_domain;	 
} zfs_fuid_domain_t;
typedef struct zfs_fuid_info {
	list_t	z_fuids;
	list_t	z_domains;
	uint64_t z_fuid_owner;
	uint64_t z_fuid_group;
	char **z_domain_table;   
	uint32_t z_fuid_cnt;	 
	uint32_t z_domain_cnt;	 
	size_t	z_domain_str_sz;  
} zfs_fuid_info_t;
#ifdef _KERNEL
struct znode;
extern uid_t zfs_fuid_map_id(zfsvfs_t *, uint64_t, cred_t *, zfs_fuid_type_t);
extern void zfs_fuid_node_add(zfs_fuid_info_t **, const char *, uint32_t,
    uint64_t, uint64_t, zfs_fuid_type_t);
extern void zfs_fuid_destroy(zfsvfs_t *);
extern uint64_t zfs_fuid_create_cred(zfsvfs_t *, zfs_fuid_type_t,
    cred_t *, zfs_fuid_info_t **);
extern uint64_t zfs_fuid_create(zfsvfs_t *, uint64_t, cred_t *, zfs_fuid_type_t,
    zfs_fuid_info_t **);
extern void zfs_fuid_map_ids(struct znode *zp, cred_t *cr,
    uid_t *uid, uid_t *gid);
extern zfs_fuid_info_t *zfs_fuid_info_alloc(void);
extern void zfs_fuid_info_free(zfs_fuid_info_t *);
extern boolean_t zfs_groupmember(zfsvfs_t *, uint64_t, cred_t *);
void zfs_fuid_sync(zfsvfs_t *, dmu_tx_t *);
extern const char *zfs_fuid_find_by_idx(zfsvfs_t *zfsvfs, uint32_t idx);
extern void zfs_fuid_txhold(zfsvfs_t *zfsvfs, dmu_tx_t *tx);
extern int zfs_id_to_fuidstr(zfsvfs_t *zfsvfs, const char *domain, uid_t rid,
    char *buf, size_t len, boolean_t addok);
#endif
const char *zfs_fuid_idx_domain(avl_tree_t *, uint32_t);
void zfs_fuid_avl_tree_create(avl_tree_t *, avl_tree_t *);
uint64_t zfs_fuid_table_load(objset_t *, uint64_t, avl_tree_t *, avl_tree_t *);
void zfs_fuid_table_destroy(avl_tree_t *, avl_tree_t *);
#ifdef	__cplusplus
}
#endif
#endif	 
