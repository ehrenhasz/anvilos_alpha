

#ifndef __XFS_FS_H__
#define __XFS_FS_H__




#ifndef HAVE_DIOATTR
struct dioattr {
	__u32		d_mem;		
	__u32		d_miniosz;	
	__u32		d_maxiosz;	
};
#endif


#ifndef HAVE_GETBMAP
struct getbmap {
	__s64		bmv_offset;	
	__s64		bmv_block;	
	__s64		bmv_length;	
	__s32		bmv_count;	
	__s32		bmv_entries;	
};
#endif


#ifndef HAVE_GETBMAPX
struct getbmapx {
	__s64		bmv_offset;	
	__s64		bmv_block;	
	__s64		bmv_length;	
	__s32		bmv_count;	
	__s32		bmv_entries;	
	__s32		bmv_iflags;	
	__s32		bmv_oflags;	
	__s32		bmv_unused1;	
	__s32		bmv_unused2;	
};
#endif


#define BMV_IF_ATTRFORK		0x1	
#define BMV_IF_NO_DMAPI_READ	0x2	
#define BMV_IF_PREALLOC		0x4	
#define BMV_IF_DELALLOC		0x8	
#define BMV_IF_NO_HOLES		0x10	
#define BMV_IF_COWFORK		0x20	
#define BMV_IF_VALID	\
	(BMV_IF_ATTRFORK|BMV_IF_NO_DMAPI_READ|BMV_IF_PREALLOC|	\
	 BMV_IF_DELALLOC|BMV_IF_NO_HOLES|BMV_IF_COWFORK)


#define BMV_OF_PREALLOC		0x1	
#define BMV_OF_DELALLOC		0x2	
#define BMV_OF_LAST		0x4	
#define BMV_OF_SHARED		0x8	


#define XFS_FMR_OWN_FREE	FMR_OWN_FREE      
#define XFS_FMR_OWN_UNKNOWN	FMR_OWN_UNKNOWN   
#define XFS_FMR_OWN_FS		FMR_OWNER('X', 1) 
#define XFS_FMR_OWN_LOG		FMR_OWNER('X', 2) 
#define XFS_FMR_OWN_AG		FMR_OWNER('X', 3) 
#define XFS_FMR_OWN_INOBT	FMR_OWNER('X', 4) 
#define XFS_FMR_OWN_INODES	FMR_OWNER('X', 5) 
#define XFS_FMR_OWN_REFC	FMR_OWNER('X', 6) 
#define XFS_FMR_OWN_COW		FMR_OWNER('X', 7) 
#define XFS_FMR_OWN_DEFECTIVE	FMR_OWNER('X', 8) 


typedef struct xfs_flock64 {
	__s16		l_type;
	__s16		l_whence;
	__s64		l_start;
	__s64		l_len;		
	__s32		l_sysid;
	__u32		l_pid;
	__s32		l_pad[4];	
} xfs_flock64_t;


struct xfs_fsop_geom_v1 {
	__u32		blocksize;	
	__u32		rtextsize;	
	__u32		agblocks;	
	__u32		agcount;	
	__u32		logblocks;	
	__u32		sectsize;	
	__u32		inodesize;	
	__u32		imaxpct;	
	__u64		datablocks;	
	__u64		rtblocks;	
	__u64		rtextents;	
	__u64		logstart;	
	unsigned char	uuid[16];	
	__u32		sunit;		
	__u32		swidth;		
	__s32		version;	
	__u32		flags;		
	__u32		logsectsize;	
	__u32		rtsectsize;	
	__u32		dirblocksize;	
};


struct xfs_fsop_geom_v4 {
	__u32		blocksize;	
	__u32		rtextsize;	
	__u32		agblocks;	
	__u32		agcount;	
	__u32		logblocks;	
	__u32		sectsize;	
	__u32		inodesize;	
	__u32		imaxpct;	
	__u64		datablocks;	
	__u64		rtblocks;	
	__u64		rtextents;	
	__u64		logstart;	
	unsigned char	uuid[16];	
	__u32		sunit;		
	__u32		swidth;		
	__s32		version;	
	__u32		flags;		
	__u32		logsectsize;	
	__u32		rtsectsize;	
	__u32		dirblocksize;	
	__u32		logsunit;	
};


struct xfs_fsop_geom {
	__u32		blocksize;	
	__u32		rtextsize;	
	__u32		agblocks;	
	__u32		agcount;	
	__u32		logblocks;	
	__u32		sectsize;	
	__u32		inodesize;	
	__u32		imaxpct;	
	__u64		datablocks;	
	__u64		rtblocks;	
	__u64		rtextents;	
	__u64		logstart;	
	unsigned char	uuid[16];	
	__u32		sunit;		
	__u32		swidth;		
	__s32		version;	
	__u32		flags;		
	__u32		logsectsize;	
	__u32		rtsectsize;	
	__u32		dirblocksize;	
	__u32		logsunit;	
	uint32_t	sick;		
	uint32_t	checked;	
	__u64		reserved[17];	
};

#define XFS_FSOP_GEOM_SICK_COUNTERS	(1 << 0)  
#define XFS_FSOP_GEOM_SICK_UQUOTA	(1 << 1)  
#define XFS_FSOP_GEOM_SICK_GQUOTA	(1 << 2)  
#define XFS_FSOP_GEOM_SICK_PQUOTA	(1 << 3)  
#define XFS_FSOP_GEOM_SICK_RT_BITMAP	(1 << 4)  
#define XFS_FSOP_GEOM_SICK_RT_SUMMARY	(1 << 5)  


typedef struct xfs_fsop_counts {
	__u64	freedata;	
	__u64	freertx;	
	__u64	freeino;	
	__u64	allocino;	
} xfs_fsop_counts_t;


typedef struct xfs_fsop_resblks {
	__u64  resblks;
	__u64  resblks_avail;
} xfs_fsop_resblks_t;

#define XFS_FSOP_GEOM_VERSION		0
#define XFS_FSOP_GEOM_VERSION_V5	5

#define XFS_FSOP_GEOM_FLAGS_ATTR	(1 << 0)  
#define XFS_FSOP_GEOM_FLAGS_NLINK	(1 << 1)  
#define XFS_FSOP_GEOM_FLAGS_QUOTA	(1 << 2)  
#define XFS_FSOP_GEOM_FLAGS_IALIGN	(1 << 3)  
#define XFS_FSOP_GEOM_FLAGS_DALIGN	(1 << 4)  
#define XFS_FSOP_GEOM_FLAGS_SHARED	(1 << 5)  
#define XFS_FSOP_GEOM_FLAGS_EXTFLG	(1 << 6)  
#define XFS_FSOP_GEOM_FLAGS_DIRV2	(1 << 7)  
#define XFS_FSOP_GEOM_FLAGS_LOGV2	(1 << 8)  
#define XFS_FSOP_GEOM_FLAGS_SECTOR	(1 << 9)  
#define XFS_FSOP_GEOM_FLAGS_ATTR2	(1 << 10) 
#define XFS_FSOP_GEOM_FLAGS_PROJID32	(1 << 11) 
#define XFS_FSOP_GEOM_FLAGS_DIRV2CI	(1 << 12) 
	
#define XFS_FSOP_GEOM_FLAGS_LAZYSB	(1 << 14) 
#define XFS_FSOP_GEOM_FLAGS_V5SB	(1 << 15) 
#define XFS_FSOP_GEOM_FLAGS_FTYPE	(1 << 16) 
#define XFS_FSOP_GEOM_FLAGS_FINOBT	(1 << 17) 
#define XFS_FSOP_GEOM_FLAGS_SPINODES	(1 << 18) 
#define XFS_FSOP_GEOM_FLAGS_RMAPBT	(1 << 19) 
#define XFS_FSOP_GEOM_FLAGS_REFLINK	(1 << 20) 
#define XFS_FSOP_GEOM_FLAGS_BIGTIME	(1 << 21) 
#define XFS_FSOP_GEOM_FLAGS_INOBTCNT	(1 << 22) 
#define XFS_FSOP_GEOM_FLAGS_NREXT64	(1 << 23) 


#define XFS_MIN_AG_BLOCKS	64
#define XFS_MIN_LOG_BLOCKS	512ULL
#define XFS_MAX_LOG_BLOCKS	(1024 * 1024ULL)
#define XFS_MIN_LOG_BYTES	(10 * 1024 * 1024ULL)


#define XFS_MIN_AG_BYTES	(1ULL << 24)	
#define XFS_MAX_AG_BYTES	(1ULL << 40)	
#define XFS_MAX_AG_BLOCKS	(XFS_MAX_AG_BYTES / XFS_MIN_BLOCKSIZE)
#define XFS_MAX_CRC_AG_BLOCKS	(XFS_MAX_AG_BYTES / XFS_MIN_CRC_BLOCKSIZE)

#define XFS_MAX_AGNUMBER	((xfs_agnumber_t)(NULLAGNUMBER - 1))


#define XFS_MAX_LOG_BYTES \
	((2 * 1024 * 1024 * 1024ULL) - XFS_MIN_LOG_BYTES)


#define XFS_MAX_DBLOCKS(s) ((xfs_rfsblock_t)(s)->sb_agcount * (s)->sb_agblocks)
#define XFS_MIN_DBLOCKS(s) ((xfs_rfsblock_t)((s)->sb_agcount - 1) *	\
			 (s)->sb_agblocks + XFS_MIN_AG_BLOCKS)


struct xfs_ag_geometry {
	uint32_t	ag_number;	
	uint32_t	ag_length;	
	uint32_t	ag_freeblks;	
	uint32_t	ag_icount;	
	uint32_t	ag_ifree;	
	uint32_t	ag_sick;	
	uint32_t	ag_checked;	
	uint32_t	ag_flags;	
	uint64_t	ag_reserved[12];
};
#define XFS_AG_GEOM_SICK_SB	(1 << 0)  
#define XFS_AG_GEOM_SICK_AGF	(1 << 1)  
#define XFS_AG_GEOM_SICK_AGFL	(1 << 2)  
#define XFS_AG_GEOM_SICK_AGI	(1 << 3)  
#define XFS_AG_GEOM_SICK_BNOBT	(1 << 4)  
#define XFS_AG_GEOM_SICK_CNTBT	(1 << 5)  
#define XFS_AG_GEOM_SICK_INOBT	(1 << 6)  
#define XFS_AG_GEOM_SICK_FINOBT	(1 << 7)  
#define XFS_AG_GEOM_SICK_RMAPBT	(1 << 8)  
#define XFS_AG_GEOM_SICK_REFCNTBT (1 << 9)  


typedef struct xfs_growfs_data {
	__u64		newblocks;	
	__u32		imaxpct;	
} xfs_growfs_data_t;

typedef struct xfs_growfs_log {
	__u32		newblocks;	
	__u32		isint;		
} xfs_growfs_log_t;

typedef struct xfs_growfs_rt {
	__u64		newblocks;	
	__u32		extsize;	
} xfs_growfs_rt_t;



typedef struct xfs_bstime {
	__kernel_long_t tv_sec;		
	__s32		tv_nsec;	
} xfs_bstime_t;

struct xfs_bstat {
	__u64		bs_ino;		
	__u16		bs_mode;	
	__u16		bs_nlink;	
	__u32		bs_uid;		
	__u32		bs_gid;		
	__u32		bs_rdev;	
	__s32		bs_blksize;	
	__s64		bs_size;	
	xfs_bstime_t	bs_atime;	
	xfs_bstime_t	bs_mtime;	
	xfs_bstime_t	bs_ctime;	
	int64_t		bs_blocks;	
	__u32		bs_xflags;	
	__s32		bs_extsize;	
	__s32		bs_extents;	
	__u32		bs_gen;		
	__u16		bs_projid_lo;	
#define	bs_projid	bs_projid_lo	
	__u16		bs_forkoff;	
	__u16		bs_projid_hi;	
	uint16_t	bs_sick;	
	uint16_t	bs_checked;	
	unsigned char	bs_pad[2];	
	__u32		bs_cowextsize;	
	__u32		bs_dmevmask;	
	__u16		bs_dmstate;	
	__u16		bs_aextents;	
};


struct xfs_bulkstat {
	uint64_t	bs_ino;		
	uint64_t	bs_size;	

	uint64_t	bs_blocks;	
	uint64_t	bs_xflags;	

	int64_t		bs_atime;	
	int64_t		bs_mtime;	

	int64_t		bs_ctime;	
	int64_t		bs_btime;	

	uint32_t	bs_gen;		
	uint32_t	bs_uid;		
	uint32_t	bs_gid;		
	uint32_t	bs_projectid;	

	uint32_t	bs_atime_nsec;	
	uint32_t	bs_mtime_nsec;	
	uint32_t	bs_ctime_nsec;	
	uint32_t	bs_btime_nsec;	

	uint32_t	bs_blksize;	
	uint32_t	bs_rdev;	
	uint32_t	bs_cowextsize_blks; 
	uint32_t	bs_extsize_blks; 

	uint32_t	bs_nlink;	
	uint32_t	bs_extents;	
	uint32_t	bs_aextents;	
	uint16_t	bs_version;	
	uint16_t	bs_forkoff;	

	uint16_t	bs_sick;	
	uint16_t	bs_checked;	
	uint16_t	bs_mode;	
	uint16_t	bs_pad2;	
	uint64_t	bs_extents64;	

	uint64_t	bs_pad[6];	
};

#define XFS_BULKSTAT_VERSION_V1	(1)
#define XFS_BULKSTAT_VERSION_V5	(5)


#define XFS_BS_SICK_INODE	(1 << 0)  
#define XFS_BS_SICK_BMBTD	(1 << 1)  
#define XFS_BS_SICK_BMBTA	(1 << 2)  
#define XFS_BS_SICK_BMBTC	(1 << 3)  
#define XFS_BS_SICK_DIR		(1 << 4)  
#define XFS_BS_SICK_XATTR	(1 << 5)  
#define XFS_BS_SICK_SYMLINK	(1 << 6)  
#define XFS_BS_SICK_PARENT	(1 << 7)  


static inline uint32_t
bstat_get_projid(const struct xfs_bstat *bs)
{
	return (uint32_t)bs->bs_projid_hi << 16 | bs->bs_projid_lo;
}


struct xfs_fsop_bulkreq {
	__u64		__user *lastip;	
	__s32		icount;		
	void		__user *ubuffer;
	__s32		__user *ocount;	
};


struct xfs_inogrp {
	__u64		xi_startino;	
	__s32		xi_alloccount;	
	__u64		xi_allocmask;	
};


struct xfs_inumbers {
	uint64_t	xi_startino;	
	uint64_t	xi_allocmask;	
	uint8_t		xi_alloccount;	
	uint8_t		xi_version;	
	uint8_t		xi_padding[6];	
};

#define XFS_INUMBERS_VERSION_V1	(1)
#define XFS_INUMBERS_VERSION_V5	(5)


struct xfs_bulk_ireq {
	uint64_t	ino;		
	uint32_t	flags;		
	uint32_t	icount;		
	uint32_t	ocount;		
	uint32_t	agno;		
	uint64_t	reserved[5];	
};


#define XFS_BULK_IREQ_AGNO	(1U << 0)


#define XFS_BULK_IREQ_SPECIAL	(1U << 1)


#define XFS_BULK_IREQ_NREXT64	(1U << 2)

#define XFS_BULK_IREQ_FLAGS_ALL	(XFS_BULK_IREQ_AGNO |	 \
				 XFS_BULK_IREQ_SPECIAL | \
				 XFS_BULK_IREQ_NREXT64)


#define XFS_BULK_IREQ_SPECIAL_ROOT	(1)


struct xfs_bulkstat_req {
	struct xfs_bulk_ireq	hdr;
	struct xfs_bulkstat	bulkstat[];
};
#define XFS_BULKSTAT_REQ_SIZE(nr)	(sizeof(struct xfs_bulkstat_req) + \
					 (nr) * sizeof(struct xfs_bulkstat))

struct xfs_inumbers_req {
	struct xfs_bulk_ireq	hdr;
	struct xfs_inumbers	inumbers[];
};
#define XFS_INUMBERS_REQ_SIZE(nr)	(sizeof(struct xfs_inumbers_req) + \
					 (nr) * sizeof(struct xfs_inumbers))


typedef struct xfs_error_injection {
	__s32		fd;
	__s32		errtag;
} xfs_error_injection_t;



#define XFS_EOFBLOCKS_VERSION		1
struct xfs_fs_eofblocks {
	__u32		eof_version;
	__u32		eof_flags;
	uid_t		eof_uid;
	gid_t		eof_gid;
	prid_t		eof_prid;
	__u32		pad32;
	__u64		eof_min_file_size;
	__u64		pad64[12];
};


#define XFS_EOF_FLAGS_SYNC		(1 << 0) 
#define XFS_EOF_FLAGS_UID		(1 << 1) 
#define XFS_EOF_FLAGS_GID		(1 << 2) 
#define XFS_EOF_FLAGS_PRID		(1 << 3) 
#define XFS_EOF_FLAGS_MINFILESIZE	(1 << 4) 
#define XFS_EOF_FLAGS_UNION		(1 << 5) 
#define XFS_EOF_FLAGS_VALID	\
	(XFS_EOF_FLAGS_SYNC |	\
	 XFS_EOF_FLAGS_UID |	\
	 XFS_EOF_FLAGS_GID |	\
	 XFS_EOF_FLAGS_PRID |	\
	 XFS_EOF_FLAGS_MINFILESIZE)



typedef struct xfs_fsop_handlereq {
	__u32		fd;		
	void		__user *path;	
	__u32		oflags;		
	void		__user *ihandle;
	__u32		ihandlen;	
	void		__user *ohandle;
	__u32		__user *ohandlen;
} xfs_fsop_handlereq_t;




#define XFS_IOC_ATTR_ROOT	0x0002	
#define XFS_IOC_ATTR_SECURE	0x0008	
#define XFS_IOC_ATTR_CREATE	0x0010	
#define XFS_IOC_ATTR_REPLACE	0x0020	

typedef struct xfs_attrlist_cursor {
	__u32		opaque[4];
} xfs_attrlist_cursor_t;


struct xfs_attrlist {
	__s32	al_count;	
	__s32	al_more;	
	__s32	al_offset[];	
};

struct xfs_attrlist_ent {	
	__u32	a_valuelen;	
	char	a_name[];	
};

typedef struct xfs_fsop_attrlist_handlereq {
	struct xfs_fsop_handlereq	hreq; 
	struct xfs_attrlist_cursor	pos; 
	__u32				flags;	
	__u32				buflen;	
	void				__user *buffer;	
} xfs_fsop_attrlist_handlereq_t;

typedef struct xfs_attr_multiop {
	__u32		am_opcode;
#define ATTR_OP_GET	1	
#define ATTR_OP_SET	2	
#define ATTR_OP_REMOVE	3	
	__s32		am_error;
	void		__user *am_attrname;
	void		__user *am_attrvalue;
	__u32		am_length;
	__u32		am_flags; 
} xfs_attr_multiop_t;

typedef struct xfs_fsop_attrmulti_handlereq {
	struct xfs_fsop_handlereq	hreq; 
	__u32				opcount;
	struct xfs_attr_multiop		__user *ops; 
} xfs_fsop_attrmulti_handlereq_t;


typedef struct { __u32 val[2]; } xfs_fsid_t; 

typedef struct xfs_fid {
	__u16	fid_len;		
	__u16	fid_pad;
	__u32	fid_gen;		
	__u64	fid_ino;		
} xfs_fid_t;

typedef struct xfs_handle {
	union {
		__s64	    align;	
		xfs_fsid_t  _ha_fsid;	
	} ha_u;
	xfs_fid_t	ha_fid;		
} xfs_handle_t;
#define ha_fsid ha_u._ha_fsid


typedef struct xfs_swapext
{
	int64_t		sx_version;	
#define XFS_SX_VERSION		0
	int64_t		sx_fdtarget;	
	int64_t		sx_fdtmp;	
	xfs_off_t	sx_offset;	
	xfs_off_t	sx_length;	
	char		sx_pad[16];	
	struct xfs_bstat sx_stat;	
} xfs_swapext_t;


#define XFS_FSOP_GOING_FLAGS_DEFAULT		0x0	
#define XFS_FSOP_GOING_FLAGS_LOGFLUSH		0x1	
#define XFS_FSOP_GOING_FLAGS_NOLOGFLUSH		0x2	


struct xfs_scrub_metadata {
	__u32 sm_type;		
	__u32 sm_flags;		
	__u64 sm_ino;		
	__u32 sm_gen;		
	__u32 sm_agno;		
	__u64 sm_reserved[5];	
};




#define XFS_SCRUB_TYPE_PROBE	0	
#define XFS_SCRUB_TYPE_SB	1	
#define XFS_SCRUB_TYPE_AGF	2	
#define XFS_SCRUB_TYPE_AGFL	3	
#define XFS_SCRUB_TYPE_AGI	4	
#define XFS_SCRUB_TYPE_BNOBT	5	
#define XFS_SCRUB_TYPE_CNTBT	6	
#define XFS_SCRUB_TYPE_INOBT	7	
#define XFS_SCRUB_TYPE_FINOBT	8	
#define XFS_SCRUB_TYPE_RMAPBT	9	
#define XFS_SCRUB_TYPE_REFCNTBT	10	
#define XFS_SCRUB_TYPE_INODE	11	
#define XFS_SCRUB_TYPE_BMBTD	12	
#define XFS_SCRUB_TYPE_BMBTA	13	
#define XFS_SCRUB_TYPE_BMBTC	14	
#define XFS_SCRUB_TYPE_DIR	15	
#define XFS_SCRUB_TYPE_XATTR	16	
#define XFS_SCRUB_TYPE_SYMLINK	17	
#define XFS_SCRUB_TYPE_PARENT	18	
#define XFS_SCRUB_TYPE_RTBITMAP	19	
#define XFS_SCRUB_TYPE_RTSUM	20	
#define XFS_SCRUB_TYPE_UQUOTA	21	
#define XFS_SCRUB_TYPE_GQUOTA	22	
#define XFS_SCRUB_TYPE_PQUOTA	23	
#define XFS_SCRUB_TYPE_FSCOUNTERS 24	


#define XFS_SCRUB_TYPE_NR	25


#define XFS_SCRUB_IFLAG_REPAIR		(1u << 0)


#define XFS_SCRUB_OFLAG_CORRUPT		(1u << 1)


#define XFS_SCRUB_OFLAG_PREEN		(1u << 2)


#define XFS_SCRUB_OFLAG_XFAIL		(1u << 3)


#define XFS_SCRUB_OFLAG_XCORRUPT	(1u << 4)


#define XFS_SCRUB_OFLAG_INCOMPLETE	(1u << 5)


#define XFS_SCRUB_OFLAG_WARNING		(1u << 6)


#define XFS_SCRUB_OFLAG_NO_REPAIR_NEEDED (1u << 7)


#define XFS_SCRUB_IFLAG_FORCE_REBUILD	(1u << 8)

#define XFS_SCRUB_FLAGS_IN	(XFS_SCRUB_IFLAG_REPAIR | \
				 XFS_SCRUB_IFLAG_FORCE_REBUILD)
#define XFS_SCRUB_FLAGS_OUT	(XFS_SCRUB_OFLAG_CORRUPT | \
				 XFS_SCRUB_OFLAG_PREEN | \
				 XFS_SCRUB_OFLAG_XFAIL | \
				 XFS_SCRUB_OFLAG_XCORRUPT | \
				 XFS_SCRUB_OFLAG_INCOMPLETE | \
				 XFS_SCRUB_OFLAG_WARNING | \
				 XFS_SCRUB_OFLAG_NO_REPAIR_NEEDED)
#define XFS_SCRUB_FLAGS_ALL	(XFS_SCRUB_FLAGS_IN | XFS_SCRUB_FLAGS_OUT)


#ifdef XATTR_LIST_MAX
#  define XFS_XATTR_LIST_MAX XATTR_LIST_MAX
#else
#  define XFS_XATTR_LIST_MAX 65536
#endif



#define XFS_IOC_GETXFLAGS	FS_IOC_GETFLAGS
#define XFS_IOC_SETXFLAGS	FS_IOC_SETFLAGS
#define XFS_IOC_GETVERSION	FS_IOC_GETVERSION




#define XFS_IOC_DIOINFO		_IOR ('X', 30, struct dioattr)
#define XFS_IOC_FSGETXATTR	FS_IOC_FSGETXATTR
#define XFS_IOC_FSSETXATTR	FS_IOC_FSSETXATTR


#define XFS_IOC_GETBMAP		_IOWR('X', 38, struct getbmap)

#define XFS_IOC_RESVSP		_IOW ('X', 40, struct xfs_flock64)
#define XFS_IOC_UNRESVSP	_IOW ('X', 41, struct xfs_flock64)
#define XFS_IOC_RESVSP64	_IOW ('X', 42, struct xfs_flock64)
#define XFS_IOC_UNRESVSP64	_IOW ('X', 43, struct xfs_flock64)
#define XFS_IOC_GETBMAPA	_IOWR('X', 44, struct getbmap)
#define XFS_IOC_FSGETXATTRA	_IOR ('X', 45, struct fsxattr)


#define XFS_IOC_GETBMAPX	_IOWR('X', 56, struct getbmap)
#define XFS_IOC_ZERO_RANGE	_IOW ('X', 57, struct xfs_flock64)
#define XFS_IOC_FREE_EOFBLOCKS	_IOR ('X', 58, struct xfs_fs_eofblocks)

#define XFS_IOC_SCRUB_METADATA	_IOWR('X', 60, struct xfs_scrub_metadata)
#define XFS_IOC_AG_GEOMETRY	_IOWR('X', 61, struct xfs_ag_geometry)


#define XFS_IOC_FSGEOMETRY_V1	     _IOR ('X', 100, struct xfs_fsop_geom_v1)
#define XFS_IOC_FSBULKSTAT	     _IOWR('X', 101, struct xfs_fsop_bulkreq)
#define XFS_IOC_FSBULKSTAT_SINGLE    _IOWR('X', 102, struct xfs_fsop_bulkreq)
#define XFS_IOC_FSINUMBERS	     _IOWR('X', 103, struct xfs_fsop_bulkreq)
#define XFS_IOC_PATH_TO_FSHANDLE     _IOWR('X', 104, struct xfs_fsop_handlereq)
#define XFS_IOC_PATH_TO_HANDLE	     _IOWR('X', 105, struct xfs_fsop_handlereq)
#define XFS_IOC_FD_TO_HANDLE	     _IOWR('X', 106, struct xfs_fsop_handlereq)
#define XFS_IOC_OPEN_BY_HANDLE	     _IOWR('X', 107, struct xfs_fsop_handlereq)
#define XFS_IOC_READLINK_BY_HANDLE   _IOWR('X', 108, struct xfs_fsop_handlereq)
#define XFS_IOC_SWAPEXT		     _IOWR('X', 109, struct xfs_swapext)
#define XFS_IOC_FSGROWFSDATA	     _IOW ('X', 110, struct xfs_growfs_data)
#define XFS_IOC_FSGROWFSLOG	     _IOW ('X', 111, struct xfs_growfs_log)
#define XFS_IOC_FSGROWFSRT	     _IOW ('X', 112, struct xfs_growfs_rt)
#define XFS_IOC_FSCOUNTS	     _IOR ('X', 113, struct xfs_fsop_counts)
#define XFS_IOC_SET_RESBLKS	     _IOWR('X', 114, struct xfs_fsop_resblks)
#define XFS_IOC_GET_RESBLKS	     _IOR ('X', 115, struct xfs_fsop_resblks)
#define XFS_IOC_ERROR_INJECTION	     _IOW ('X', 116, struct xfs_error_injection)
#define XFS_IOC_ERROR_CLEARALL	     _IOW ('X', 117, struct xfs_error_injection)


#define XFS_IOC_FREEZE		     _IOWR('X', 119, int)	
#define XFS_IOC_THAW		     _IOWR('X', 120, int)	


#define XFS_IOC_ATTRLIST_BY_HANDLE   _IOW ('X', 122, struct xfs_fsop_attrlist_handlereq)
#define XFS_IOC_ATTRMULTI_BY_HANDLE  _IOW ('X', 123, struct xfs_fsop_attrmulti_handlereq)
#define XFS_IOC_FSGEOMETRY_V4	     _IOR ('X', 124, struct xfs_fsop_geom_v4)
#define XFS_IOC_GOINGDOWN	     _IOR ('X', 125, uint32_t)
#define XFS_IOC_FSGEOMETRY	     _IOR ('X', 126, struct xfs_fsop_geom)
#define XFS_IOC_BULKSTAT	     _IOR ('X', 127, struct xfs_bulkstat_req)
#define XFS_IOC_INUMBERS	     _IOR ('X', 128, struct xfs_inumbers_req)



#ifndef HAVE_BBMACROS

#define BBSHIFT		9
#define BBSIZE		(1<<BBSHIFT)
#define BBMASK		(BBSIZE-1)
#define BTOBB(bytes)	(((__u64)(bytes) + BBSIZE - 1) >> BBSHIFT)
#define BTOBBT(bytes)	((__u64)(bytes) >> BBSHIFT)
#define BBTOB(bbs)	((bbs) << BBSHIFT)
#endif

#endif	
