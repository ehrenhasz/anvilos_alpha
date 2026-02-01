 
 
#ifndef _VXFS_INODE_H_
#define _VXFS_INODE_H_

 


#define VXFS_ISIZE		0x100		 

#define VXFS_NDADDR		10		 
#define VXFS_NIADDR		2		 
#define VXFS_NIMMED		96		 
#define VXFS_NTYPED		6		 

#define VXFS_TYPED_OFFSETMASK	(0x00FFFFFFFFFFFFFFULL)
#define VXFS_TYPED_TYPEMASK	(0xFF00000000000000ULL)
#define VXFS_TYPED_TYPESHIFT	56

#define VXFS_TYPED_PER_BLOCK(sbp) \
	((sbp)->s_blocksize / sizeof(struct vxfs_typed))

 
enum {
	VXFS_TYPED_INDIRECT		= 1,
	VXFS_TYPED_DATA			= 2,
	VXFS_TYPED_INDIRECT_DEV4	= 3,
	VXFS_TYPED_DATA_DEV4		= 4,
};

 
struct vxfs_immed {
	__u8			vi_immed[VXFS_NIMMED];
};

struct vxfs_ext4 {
	__fs32			ve4_spare;		 
	__fs32			ve4_indsize;		 
	__fs32			ve4_indir[VXFS_NIADDR];	 
	struct direct {					 
		__fs32		extent;			 
		__fs32		size;			 
	} ve4_direct[VXFS_NDADDR];
};

struct vxfs_typed {
	__fs64		vt_hdr;		 
	__fs32		vt_block;	 
	__fs32		vt_size;	 
};

struct vxfs_typed_dev4 {
	__fs64		vd4_hdr;	 
	__fs64		vd4_block;	 
	__fs64		vd4_size;	 
	__fs32		vd4_dev;	 
	__u8		__pad1;
};

 
struct vxfs_dinode {
	__fs32		vdi_mode;
	__fs32		vdi_nlink;	 
	__fs32		vdi_uid;	 
	__fs32		vdi_gid;	 
	__fs64		vdi_size;	 
	__fs32		vdi_atime;	 
	__fs32		vdi_autime;	 
	__fs32		vdi_mtime;	 
	__fs32		vdi_mutime;	 
	__fs32		vdi_ctime;	 
	__fs32		vdi_cutime;	 
	__u8		vdi_aflags;	 
	__u8		vdi_orgtype;	 
	__fs16		vdi_eopflags;
	__fs32		vdi_eopdata;
	union {
		__fs32			rdev;
		__fs32			dotdot;
		struct {
			__u32		reserved;
			__fs32		fixextsize;
		} i_regular;
		struct {
			__fs32		matchino;
			__fs32		fsetindex;
		} i_vxspec;
		__u64			align;
	} vdi_ftarea;
	__fs32		vdi_blocks;	 
	__fs32		vdi_gen;	 
	__fs64		vdi_version;	 
	union {
		struct vxfs_immed	immed;
		struct vxfs_ext4	ext4;
		struct vxfs_typed	typed[VXFS_NTYPED];
	} vdi_org;
	__fs32		vdi_iattrino;
};

#define vdi_rdev	vdi_ftarea.rdev
#define vdi_dotdot	vdi_ftarea.dotdot
#define vdi_fixextsize	vdi_ftarea.regular.fixextsize
#define vdi_matchino	vdi_ftarea.vxspec.matchino
#define vdi_fsetindex	vdi_ftarea.vxspec.fsetindex

#define vdi_immed	vdi_org.immed
#define vdi_ext4	vdi_org.ext4
#define vdi_typed	vdi_org.typed


 
struct vxfs_inode_info {
	struct inode	vfs_inode;

	__u32		vii_mode;
	__u32		vii_nlink;	 
	__u32		vii_uid;	 
	__u32		vii_gid;	 
	__u64		vii_size;	 
	__u32		vii_atime;	 
	__u32		vii_autime;	 
	__u32		vii_mtime;	 
	__u32		vii_mutime;	 
	__u32		vii_ctime;	 
	__u32		vii_cutime;	 
	__u8		vii_orgtype;	 
	union {
		__u32			rdev;
		__u32			dotdot;
	} vii_ftarea;
	__u32		vii_blocks;	 
	__u32		vii_gen;	 
	union {
		struct vxfs_immed	immed;
		struct vxfs_ext4	ext4;
		struct vxfs_typed	typed[VXFS_NTYPED];
	} vii_org;
};

#define vii_rdev	vii_ftarea.rdev
#define vii_dotdot	vii_ftarea.dotdot

#define vii_immed	vii_org.immed
#define vii_ext4	vii_org.ext4
#define vii_typed	vii_org.typed

static inline struct vxfs_inode_info *VXFS_INO(struct inode *inode)
{
	return container_of(inode, struct vxfs_inode_info, vfs_inode);
}

#endif  
