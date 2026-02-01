 
 
#ifndef	_H_JFS_SUPERBLOCK
#define _H_JFS_SUPERBLOCK

#include <linux/uuid.h>

 
#define JFS_MAGIC	"JFS1"	 

#define JFS_VERSION	2	 

#define LV_NAME_SIZE	11	 

 
struct jfs_superblock {
	char s_magic[4];	 
	__le32 s_version;	 

	__le64 s_size;		 
	__le32 s_bsize;		 
	__le16 s_l2bsize;	 
	__le16 s_l2bfactor;	 
	__le32 s_pbsize;	 
	__le16 s_l2pbsize;	 
	__le16 pad;		 

	__le32 s_agsize;	 

	__le32 s_flag;		 
	__le32 s_state;		 
	__le32 s_compress;		 

	pxd_t s_ait2;		 

	pxd_t s_aim2;		 
	__le32 s_logdev;		 
	__le32 s_logserial;	 
	pxd_t s_logpxd;		 

	pxd_t s_fsckpxd;	 

	struct timestruc_t s_time;	 

	__le32 s_fsckloglen;	 
	s8 s_fscklog;		 
	char s_fpack[11];	 

	 
	__le64 s_xsize;		 
	pxd_t s_xfsckpxd;	 
	pxd_t s_xlogpxd;	 
	uuid_t s_uuid;		 
	char s_label[16];	 
	uuid_t s_loguuid;	 

};

extern int readSuper(struct super_block *, struct buffer_head **);
extern int updateSuper(struct super_block *, uint);
__printf(2, 3)
extern void jfs_error(struct super_block *, const char *, ...);
extern int jfs_mount(struct super_block *);
extern int jfs_mount_rw(struct super_block *, int);
extern int jfs_umount(struct super_block *);
extern int jfs_umount_rw(struct super_block *);
extern int jfs_extendfs(struct super_block *, s64, int);

extern struct task_struct *jfsIOthread;
extern struct task_struct *jfsSyncThread;

#endif  
