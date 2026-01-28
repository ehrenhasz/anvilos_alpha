#ifndef _VXFS_SUPER_H_
#define _VXFS_SUPER_H_
#include <linux/types.h>
#define VXFS_SUPER_MAGIC	0xa501FCF5
#define VXFS_ROOT_INO		2
#define VXFS_NEFREE		32
enum vxfs_byte_order {
	VXFS_BO_LE,
	VXFS_BO_BE,
};
typedef __u16 __bitwise __fs16;
typedef __u32 __bitwise __fs32;
typedef __u64 __bitwise __fs64;
struct vxfs_sb {
	__fs32		vs_magic;		 
	__fs32		vs_version;		 
	__fs32		vs_ctime;		 
	__fs32		vs_cutime;		 
	__fs32		__unused1;		 
	__fs32		__unused2;		 
	__fs32		vs_old_logstart;	 
	__fs32		vs_old_logend;		 
	__fs32		vs_bsize;		 
	__fs32		vs_size;		 
	__fs32		vs_dsize;		 
	__fs32		vs_old_ninode;		 
	__fs32		vs_old_nau;		 
	__fs32		__unused3;		 
	__fs32		vs_old_defiextsize;	 
	__fs32		vs_old_ilbsize;		 
	__fs32		vs_immedlen;		 
	__fs32		vs_ndaddr;		 
	__fs32		vs_firstau;		 
	__fs32		vs_emap;		 
	__fs32		vs_imap;		 
	__fs32		vs_iextop;		 
	__fs32		vs_istart;		 
	__fs32		vs_bstart;		 
	__fs32		vs_femap;		 
	__fs32		vs_fimap;		 
	__fs32		vs_fiextop;		 
	__fs32		vs_fistart;		 
	__fs32		vs_fbstart;		 
	__fs32		vs_nindir;		 
	__fs32		vs_aulen;		 
	__fs32		vs_auimlen;		 
	__fs32		vs_auemlen;		 
	__fs32		vs_auilen;		 
	__fs32		vs_aupad;		 
	__fs32		vs_aublocks;		 
	__fs32		vs_maxtier;		 
	__fs32		vs_inopb;		 
	__fs32		vs_old_inopau;		 
	__fs32		vs_old_inopilb;		 
	__fs32		vs_old_ndiripau;	 
	__fs32		vs_iaddrlen;		 
	__fs32		vs_bshift;		 
	__fs32		vs_inoshift;		 
	__fs32		vs_bmask;		 
	__fs32		vs_boffmask;		 
	__fs32		vs_old_inomask;		 
	__fs32		vs_checksum;		 
	__fs32		vs_free;		 
	__fs32		vs_ifree;		 
	__fs32		vs_efree[VXFS_NEFREE];	 
	__fs32		vs_flags;		 
	__u8		vs_mod;			 
	__u8		vs_clean;		 
	__fs16		__unused4;		 
	__fs32		vs_firstlogid;		 
	__fs32		vs_wtime;		 
	__fs32		vs_wutime;		 
	__u8		vs_fname[6];		 
	__u8		vs_fpack[6];		 
	__fs32		vs_logversion;		 
	__u32		__unused5;		 
	__fs32		vs_oltext[2];		 
	__fs32		vs_oltsize;		 
	__fs32		vs_iauimlen;		 
	__fs32		vs_iausize;		 
	__fs32		vs_dinosize;		 
	__fs32		vs_old_dniaddr;		 
	__fs32		vs_checksum2;		 
};
struct vxfs_sb_info {
	struct vxfs_sb		*vsi_raw;	 
	struct buffer_head	*vsi_bp;	 
	struct inode		*vsi_fship;	 
	struct inode		*vsi_ilist;	 
	struct inode		*vsi_stilist;	 
	u_long			vsi_iext;	 
	ino_t			vsi_fshino;	 
	daddr_t			vsi_oltext;	 
	daddr_t			vsi_oltsize;	 
	enum vxfs_byte_order	byte_order;
};
static inline u16 fs16_to_cpu(struct vxfs_sb_info *sbi, __fs16 a)
{
	if (sbi->byte_order == VXFS_BO_BE)
		return be16_to_cpu((__force __be16)a);
	else
		return le16_to_cpu((__force __le16)a);
}
static inline u32 fs32_to_cpu(struct vxfs_sb_info *sbi, __fs32 a)
{
	if (sbi->byte_order == VXFS_BO_BE)
		return be32_to_cpu((__force __be32)a);
	else
		return le32_to_cpu((__force __le32)a);
}
static inline u64 fs64_to_cpu(struct vxfs_sb_info *sbi, __fs64 a)
{
	if (sbi->byte_order == VXFS_BO_BE)
		return be64_to_cpu((__force __be64)a);
	else
		return le64_to_cpu((__force __le64)a);
}
enum vxfs_mode {
	VXFS_ISUID = 0x00000800,	 
	VXFS_ISGID = 0x00000400,	 
	VXFS_ISVTX = 0x00000200,	 
	VXFS_IREAD = 0x00000100,	 
	VXFS_IWRITE = 0x00000080,	 
	VXFS_IEXEC = 0x00000040,	 
	VXFS_IFIFO = 0x00001000,	 
	VXFS_IFCHR = 0x00002000,	 
	VXFS_IFDIR = 0x00004000,	 
	VXFS_IFNAM = 0x00005000,	 
	VXFS_IFBLK = 0x00006000,	 
	VXFS_IFREG = 0x00008000,	 
	VXFS_IFCMP = 0x00009000,	 
	VXFS_IFLNK = 0x0000a000,	 
	VXFS_IFSOC = 0x0000c000,	 
	VXFS_IFFSH = 0x10000000,	 
	VXFS_IFILT = 0x20000000,	 
	VXFS_IFIAU = 0x30000000,	 
	VXFS_IFCUT = 0x40000000,	 
	VXFS_IFATT = 0x50000000,	 
	VXFS_IFLCT = 0x60000000,	 
	VXFS_IFIAT = 0x70000000,	 
	VXFS_IFEMR = 0x80000000,	 
	VXFS_IFQUO = 0x90000000,	 
	VXFS_IFPTI = 0xa0000000,	 
	VXFS_IFLAB = 0x11000000,	 
	VXFS_IFOLT = 0x12000000,	 
	VXFS_IFLOG = 0x13000000,	 
	VXFS_IFEMP = 0x14000000,	 
	VXFS_IFEAU = 0x15000000,	 
	VXFS_IFAUS = 0x16000000,	 
	VXFS_IFDEV = 0x17000000,	 
};
#define	VXFS_TYPE_MASK		0xfffff000
#define VXFS_IS_TYPE(ip,type)	(((ip)->vii_mode & VXFS_TYPE_MASK) == (type))
#define VXFS_ISFIFO(x)		VXFS_IS_TYPE((x),VXFS_IFIFO)
#define VXFS_ISCHR(x)		VXFS_IS_TYPE((x),VXFS_IFCHR)
#define VXFS_ISDIR(x)		VXFS_IS_TYPE((x),VXFS_IFDIR)
#define VXFS_ISNAM(x)		VXFS_IS_TYPE((x),VXFS_IFNAM)
#define VXFS_ISBLK(x)		VXFS_IS_TYPE((x),VXFS_IFBLK)
#define VXFS_ISLNK(x)		VXFS_IS_TYPE((x),VXFS_IFLNK)
#define VXFS_ISREG(x)		VXFS_IS_TYPE((x),VXFS_IFREG)
#define VXFS_ISCMP(x)		VXFS_IS_TYPE((x),VXFS_IFCMP)
#define VXFS_ISSOC(x)		VXFS_IS_TYPE((x),VXFS_IFSOC)
#define VXFS_ISFSH(x)		VXFS_IS_TYPE((x),VXFS_IFFSH)
#define VXFS_ISILT(x)		VXFS_IS_TYPE((x),VXFS_IFILT)
enum {
	VXFS_ORG_NONE	= 0,	 
	VXFS_ORG_EXT4	= 1,	 
	VXFS_ORG_IMMED	= 2,	 
	VXFS_ORG_TYPED	= 3,	 
};
#define VXFS_IS_ORG(ip,org)	((ip)->vii_orgtype == (org))
#define VXFS_ISNONE(ip)		VXFS_IS_ORG((ip), VXFS_ORG_NONE)
#define VXFS_ISEXT4(ip)		VXFS_IS_ORG((ip), VXFS_ORG_EXT4)
#define VXFS_ISIMMED(ip)	VXFS_IS_ORG((ip), VXFS_ORG_IMMED)
#define VXFS_ISTYPED(ip)	VXFS_IS_ORG((ip), VXFS_ORG_TYPED)
#define VXFS_SBI(sbp) \
	((struct vxfs_sb_info *)(sbp)->s_fs_info)
#endif  
