#ifndef	_SYS_ZFS_SA_H
#define	_SYS_ZFS_SA_H
#ifdef _KERNEL
#include <sys/types32.h>
#include <sys/list.h>
#include <sys/dmu.h>
#include <sys/zfs_acl.h>
#include <sys/zfs_znode.h>
#include <sys/sa.h>
#include <sys/zil.h>
#endif
#ifdef	__cplusplus
extern "C" {
#endif
typedef enum zpl_attr {
	ZPL_ATIME,
	ZPL_MTIME,
	ZPL_CTIME,
	ZPL_CRTIME,
	ZPL_GEN,
	ZPL_MODE,
	ZPL_SIZE,
	ZPL_PARENT,
	ZPL_LINKS,
	ZPL_XATTR,
	ZPL_RDEV,
	ZPL_FLAGS,
	ZPL_UID,
	ZPL_GID,
	ZPL_PAD,
	ZPL_ZNODE_ACL,
	ZPL_DACL_COUNT,
	ZPL_SYMLINK,
	ZPL_SCANSTAMP,
	ZPL_DACL_ACES,
	ZPL_DXATTR,
	ZPL_PROJID,
	ZPL_END
} zpl_attr_t;
#define	ZFS_OLD_ZNODE_PHYS_SIZE	0x108
#define	ZFS_SA_BASE_ATTR_SIZE	(ZFS_OLD_ZNODE_PHYS_SIZE - \
    sizeof (zfs_acl_phys_t))
#define	SA_MODE_OFFSET		0
#define	SA_SIZE_OFFSET		8
#define	SA_GEN_OFFSET		16
#define	SA_UID_OFFSET		24
#define	SA_GID_OFFSET		32
#define	SA_PARENT_OFFSET	40
#define	SA_FLAGS_OFFSET		48
#define	SA_PROJID_OFFSET	128
extern const sa_attr_reg_t zfs_attr_table[ZPL_END + 1];
typedef struct znode_phys {
	uint64_t zp_atime[2];		 
	uint64_t zp_mtime[2];		 
	uint64_t zp_ctime[2];		 
	uint64_t zp_crtime[2];		 
	uint64_t zp_gen;		 
	uint64_t zp_mode;		 
	uint64_t zp_size;		 
	uint64_t zp_parent;		 
	uint64_t zp_links;		 
	uint64_t zp_xattr;		 
	uint64_t zp_rdev;		 
	uint64_t zp_flags;		 
	uint64_t zp_uid;		 
	uint64_t zp_gid;		 
	uint64_t zp_zap;		 
	uint64_t zp_pad[3];		 
	zfs_acl_phys_t zp_acl;		 
} znode_phys_t;
#ifdef _KERNEL
#define	DXATTR_MAX_ENTRY_SIZE	(32768)
#define	DXATTR_MAX_SA_SIZE	(SPA_OLD_MAXBLOCKSIZE >> 1)
int zfs_sa_readlink(struct znode *, zfs_uio_t *);
void zfs_sa_symlink(struct znode *, char *link, int len, dmu_tx_t *);
void zfs_sa_get_scanstamp(struct znode *, xvattr_t *);
void zfs_sa_set_scanstamp(struct znode *, xvattr_t *, dmu_tx_t *);
int zfs_sa_get_xattr(struct znode *);
int zfs_sa_set_xattr(struct znode *, const char *, const void *, size_t);
void zfs_sa_upgrade(struct sa_handle  *, dmu_tx_t *);
void zfs_sa_upgrade_txholds(dmu_tx_t *, struct znode *);
void zfs_sa_init(void);
void zfs_sa_fini(void);
#endif
#ifdef	__cplusplus
}
#endif
#endif	 
