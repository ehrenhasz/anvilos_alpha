#ifndef _VXFS_DIR_H_
#define _VXFS_DIR_H_
struct vxfs_dirblk {
	__fs16		d_free;		 
	__fs16		d_nhash;	 
	__fs16		d_hash[1];	 
};
#define VXFS_NAMELEN	256
struct vxfs_direct {
	__fs32		d_ino;			 
	__fs16		d_reclen;		 
	__fs16		d_namelen;		 
	__fs16		d_hashnext;		 
	char		d_name[VXFS_NAMELEN];	 
};
#define VXFS_DIRPAD		4
#define VXFS_NAMEMIN		offsetof(struct vxfs_direct, d_name)
#define VXFS_DIRROUND(len)	((VXFS_DIRPAD + (len) - 1) & ~(VXFS_DIRPAD -1))
#define VXFS_DIRLEN(len)	(VXFS_DIRROUND(VXFS_NAMEMIN + (len)))
#define VXFS_DIRBLKOV(sbi, dbp)	\
	((sizeof(short) * fs16_to_cpu(sbi, dbp->d_nhash)) + 4)
#endif  
