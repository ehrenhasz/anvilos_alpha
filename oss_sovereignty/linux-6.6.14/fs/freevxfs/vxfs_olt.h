 
 
#ifndef _VXFS_OLT_H_
#define _VXFS_OLT_H_

 


 
#define VXFS_OLT_MAGIC		0xa504FCF5

 
enum {
	VXFS_OLT_FREE	= 1,
	VXFS_OLT_FSHEAD	= 2,
	VXFS_OLT_CUT	= 3,
	VXFS_OLT_ILIST	= 4,
	VXFS_OLT_DEV	= 5,
	VXFS_OLT_SB	= 6
};

 
struct vxfs_olt {
	__fs32		olt_magic;	 
	__fs32		olt_size;	 
	__fs32		olt_checksum;	 
	__u32		__unused1;	 
	__fs32		olt_mtime;	 
	__fs32		olt_mutime;	 
	__fs32		olt_totfree;	 
	__fs32		olt_extents[2];	 
	__fs32		olt_esize;	 
	__fs32		olt_next[2];     
	__fs32		olt_nsize;	 
	__u32		__unused2;	 
};

 
struct vxfs_oltcommon {
	__fs32		olt_type;	 
	__fs32		olt_size;	 
};

 
struct vxfs_oltfree {
	__fs32		olt_type;	 
	__fs32		olt_fsize;	 
};

 
struct vxfs_oltilist {
	__fs32	olt_type;	 
	__fs32	olt_size;	 
	__fs32		olt_iext[2];	 
};

 
struct vxfs_oltcut {
	__fs32		olt_type;	 
	__fs32		olt_size;	 
	__fs32		olt_cutino;	 
	__u8		__pad;		 
};

 
struct vxfs_oltsb {
	__fs32		olt_type;	 
	__fs32		olt_size;	 
	__fs32		olt_sbino;	 
	__u32		__unused1;	 
	__fs32		olt_logino[2];	 
	__fs32		olt_oltino[2];	 
};

 
struct vxfs_oltdev {
	__fs32		olt_type;	 
	__fs32		olt_size;	 
	__fs32		olt_devino[2];	 
};

 
struct vxfs_oltfshead {
	__fs32		olt_type;	 
	__fs32		olt_size;	 
	__fs32		olt_fsino[2];    
};

#endif  
