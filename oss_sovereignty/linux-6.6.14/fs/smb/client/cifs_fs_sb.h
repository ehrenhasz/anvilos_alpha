#include <linux/rbtree.h>
#ifndef _CIFS_FS_SB_H
#define _CIFS_FS_SB_H
#include <linux/backing-dev.h>
#define CIFS_MOUNT_NO_PERM      1  
#define CIFS_MOUNT_SET_UID      2  
#define CIFS_MOUNT_SERVER_INUM  4  
#define CIFS_MOUNT_DIRECT_IO    8  
#define CIFS_MOUNT_NO_XATTR     0x10   
#define CIFS_MOUNT_MAP_SPECIAL_CHR 0x20  
#define CIFS_MOUNT_POSIX_PATHS  0x40   
#define CIFS_MOUNT_UNX_EMUL     0x80   
#define CIFS_MOUNT_NO_BRL       0x100  
#define CIFS_MOUNT_CIFS_ACL     0x200  
#define CIFS_MOUNT_OVERR_UID    0x400  
#define CIFS_MOUNT_OVERR_GID    0x800  
#define CIFS_MOUNT_DYNPERM      0x1000  
#define CIFS_MOUNT_NOPOSIXBRL   0x2000  
#define CIFS_MOUNT_NOSSYNC      0x4000  
#define CIFS_MOUNT_FSCACHE	0x8000  
#define CIFS_MOUNT_MF_SYMLINKS	0x10000  
#define CIFS_MOUNT_MULTIUSER	0x20000  
#define CIFS_MOUNT_STRICT_IO	0x40000  
#define CIFS_MOUNT_RWPIDFORWARD	0x80000  
#define CIFS_MOUNT_POSIXACL	0x100000  
#define CIFS_MOUNT_CIFS_BACKUPUID 0x200000  
#define CIFS_MOUNT_CIFS_BACKUPGID 0x400000  
#define CIFS_MOUNT_MAP_SFM_CHR	0x800000  
#define CIFS_MOUNT_USE_PREFIX_PATH 0x1000000  
#define CIFS_MOUNT_UID_FROM_ACL 0x2000000  
#define CIFS_MOUNT_NO_HANDLE_CACHE 0x4000000  
#define CIFS_MOUNT_NO_DFS 0x8000000  
#define CIFS_MOUNT_MODE_FROM_SID 0x10000000  
#define CIFS_MOUNT_RO_CACHE	0x20000000   
#define CIFS_MOUNT_RW_CACHE	0x40000000   
#define CIFS_MOUNT_SHUTDOWN	0x80000000
struct cifs_sb_info {
	struct rb_root tlink_tree;
	spinlock_t tlink_tree_lock;
	struct tcon_link *master_tlink;
	struct nls_table *local_nls;
	struct smb3_fs_context *ctx;
	atomic_t active;
	unsigned int mnt_cifs_flags;
	struct delayed_work prune_tlinks;
	struct rcu_head rcu;
	char *prepath;
	bool mnt_cifs_serverino_autodisabled;
	struct dentry *root;
};
#endif				 
