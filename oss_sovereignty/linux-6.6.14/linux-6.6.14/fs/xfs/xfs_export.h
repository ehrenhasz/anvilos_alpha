#ifndef __XFS_EXPORT_H__
#define __XFS_EXPORT_H__
struct xfs_fid64 {
	u64 ino;
	u32 gen;
	u64 parent_ino;
	u32 parent_gen;
} __attribute__((packed));
#define XFS_FILEID_TYPE_64FLAG	0x80	 
#endif	 
