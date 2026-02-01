 
 
#ifndef _H_JFS_INCORE
#define _H_JFS_INCORE

#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/uuid.h>

#include "jfs_types.h"
#include "jfs_xtree.h"
#include "jfs_dtree.h"

 
#define JFS_SUPER_MAGIC 0x3153464a  

 
struct jfs_inode_info {
	int	fileset;	 
	uint	mode2;		 
	kuid_t	saved_uid;	 
	kgid_t	saved_gid;	 
	pxd_t	ixpxd;		 
	dxd_t	acl;		 
	dxd_t	ea;		 
	time64_t otime;		 
	uint	next_index;	 
	int	acltype;	 
	short	btorder;	 
	short	btindex;	 
	struct inode *ipimap;	 
	unsigned long cflag;	 
	u64	agstart;	 
	u16	bxflag;		 
	unchar	pad;
	signed char active_ag;	 
	lid_t	blid;		 
	lid_t	atlhead;	 
	lid_t	atltail;	 
	spinlock_t ag_lock;	 
	struct list_head anon_inode_list;  
	 
	struct rw_semaphore rdwrlock;
	 
	struct mutex commit_mutex;
	 
	struct rw_semaphore xattr_sem;
	lid_t	xtlid;		 
	union {
		struct {
			xtpage_t _xtroot;	 
			struct inomap *_imap;	 
		} file;
		struct {
			struct dir_table_slot _table[12];  
			dtroot_t _dtroot;	 
		} dir;
		struct {
			unchar _unused[16];	 
			dxd_t _dxd;		 
			 
			 
			union {
				struct {
					 
					unchar _inline[128];
					 
					unchar _inline_ea[128];
				};
				unchar _inline_all[256];
			};
		} link;
	} u;
#ifdef CONFIG_QUOTA
	struct dquot *i_dquot[MAXQUOTAS];
#endif
	u32 dev;	 
	struct inode	vfs_inode;
};
#define i_xtroot u.file._xtroot
#define i_imap u.file._imap
#define i_dirtable u.dir._table
#define i_dtroot u.dir._dtroot
#define i_inline u.link._inline
#define i_inline_ea u.link._inline_ea
#define i_inline_all u.link._inline_all

#define IREAD_LOCK(ip, subclass) \
	down_read_nested(&JFS_IP(ip)->rdwrlock, subclass)
#define IREAD_UNLOCK(ip)	up_read(&JFS_IP(ip)->rdwrlock)
#define IWRITE_LOCK(ip, subclass) \
	down_write_nested(&JFS_IP(ip)->rdwrlock, subclass)
#define IWRITE_UNLOCK(ip)	up_write(&JFS_IP(ip)->rdwrlock)

 
enum cflags {
	COMMIT_Nolink,		 
	COMMIT_Inlineea,	 
	COMMIT_Freewmap,	 
	COMMIT_Dirty,		 
	COMMIT_Dirtable,	 
	COMMIT_Stale,		 
	COMMIT_Synclist,	 
};

 
enum commit_mutex_class
{
	COMMIT_MUTEX_PARENT,
	COMMIT_MUTEX_CHILD,
	COMMIT_MUTEX_SECOND_PARENT,	 
	COMMIT_MUTEX_VICTIM		 
};

 
enum rdwrlock_class
{
	RDWRLOCK_NORMAL,
	RDWRLOCK_IMAP,
	RDWRLOCK_DMAP
};

#define set_cflag(flag, ip)	set_bit(flag, &(JFS_IP(ip)->cflag))
#define clear_cflag(flag, ip)	clear_bit(flag, &(JFS_IP(ip)->cflag))
#define test_cflag(flag, ip)	test_bit(flag, &(JFS_IP(ip)->cflag))
#define test_and_clear_cflag(flag, ip) \
	test_and_clear_bit(flag, &(JFS_IP(ip)->cflag))
 
struct jfs_sb_info {
	struct super_block *sb;		 
	unsigned long	mntflag;	 
	struct inode	*ipbmap;	 
	struct inode	*ipaimap;	 
	struct inode	*ipaimap2;	 
	struct inode	*ipimap;	 
	struct jfs_log	*log;		 
	struct list_head log_list;	 
	short		bsize;		 
	short		l2bsize;	 
	short		nbperpage;	 
	short		l2nbperpage;	 
	short		l2niperblk;	 
	dev_t		logdev;		 
	uint		aggregate;	 
	pxd_t		logpxd;		 
	pxd_t		fsckpxd;	 
	pxd_t		ait2;		 
	uuid_t		uuid;		 
	uuid_t		loguuid;	 
	 
	int		commit_state;	 
	 
	uint		gengen;		 
	uint		inostamp;	 

	 
	struct bmap	*bmap;		 
	struct nls_table *nls_tab;	 
	struct inode *direct_inode;	 
	uint		state;		 
	unsigned long	flag;		 
	uint		p_state;	 
	kuid_t		uid;		 
	kgid_t		gid;		 
	uint		umask;		 
	uint		minblks_trim;	 
};

 
#define IN_LAZYCOMMIT 1

static inline struct jfs_inode_info *JFS_IP(struct inode *inode)
{
	return container_of(inode, struct jfs_inode_info, vfs_inode);
}

static inline int jfs_dirtable_inline(struct inode *inode)
{
	return (JFS_IP(inode)->next_index <= (MAX_INLINE_DIRTABLE_ENTRY + 1));
}

static inline struct jfs_sb_info *JFS_SBI(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline int isReadOnly(struct inode *inode)
{
	if (JFS_SBI(inode->i_sb)->log)
		return 0;
	return 1;
}
#endif  
