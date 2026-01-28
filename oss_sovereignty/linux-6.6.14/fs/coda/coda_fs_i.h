#ifndef _LINUX_CODA_FS_I
#define _LINUX_CODA_FS_I
#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/coda.h>
struct coda_inode_info {
	struct CodaFid	   c_fid;	 
	u_short	           c_flags;      
	unsigned int	   c_mapcount;   
	unsigned int	   c_cached_epoch;  
	kuid_t		   c_uid;	 
	unsigned int       c_cached_perm;  
	spinlock_t	   c_lock;
	struct inode	   vfs_inode;
};
#define CODA_MAGIC 0xC0DAC0DA
struct coda_file_info {
	int		   cfi_magic;	   
	struct file	  *cfi_container;  
	unsigned int	   cfi_mapcount;   
	bool		   cfi_access_intent;  
};
#define C_VATTR       0x1    
#define C_FLUSH       0x2    
#define C_DYING       0x4    
#define C_PURGE       0x8
struct inode *coda_cnode_make(struct CodaFid *, struct super_block *);
struct inode *coda_iget(struct super_block *sb, struct CodaFid *fid, struct coda_vattr *attr);
struct inode *coda_cnode_makectl(struct super_block *sb);
struct inode *coda_fid_to_inode(struct CodaFid *fid, struct super_block *sb);
struct coda_file_info *coda_ftoc(struct file *file);
void coda_replace_fid(struct inode *, struct CodaFid *, struct CodaFid *);
#endif
