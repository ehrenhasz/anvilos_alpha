

#ifndef	__XFS_LOG_FORMAT_H__
#define __XFS_LOG_FORMAT_H__

struct xfs_mount;
struct xfs_trans_res;


typedef uint32_t xlog_tid_t;

#define XLOG_MIN_ICLOGS		2
#define XLOG_MAX_ICLOGS		8
#define XLOG_HEADER_MAGIC_NUM	0xFEEDbabe	
#define XLOG_VERSION_1		1
#define XLOG_VERSION_2		2		
#define XLOG_VERSION_OKBITS	(XLOG_VERSION_1 | XLOG_VERSION_2)
#define XLOG_MIN_RECORD_BSIZE	(16*1024)	
#define XLOG_BIG_RECORD_BSIZE	(32*1024)	
#define XLOG_MAX_RECORD_BSIZE	(256*1024)
#define XLOG_HEADER_CYCLE_SIZE	(32*1024)	
#define XLOG_MIN_RECORD_BSHIFT	14		
#define XLOG_BIG_RECORD_BSHIFT	15		
#define XLOG_MAX_RECORD_BSHIFT	18		

#define XLOG_HEADER_SIZE	512


#define XFS_MIN_LOG_FACTOR	3

#define XLOG_REC_SHIFT(log) \
	BTOBB(1 << (xfs_has_logv2(log->l_mp) ? \
	 XLOG_MAX_RECORD_BSHIFT : XLOG_BIG_RECORD_BSHIFT))
#define XLOG_TOTAL_REC_SHIFT(log) \
	BTOBB(XLOG_MAX_ICLOGS << (xfs_has_logv2(log->l_mp) ? \
	 XLOG_MAX_RECORD_BSHIFT : XLOG_BIG_RECORD_BSHIFT))


#define CYCLE_LSN(lsn) ((uint)((lsn)>>32))
#define BLOCK_LSN(lsn) ((uint)(lsn))


#define CYCLE_LSN_DISK(lsn) (((__be32 *)&(lsn))[0])

static inline xfs_lsn_t xlog_assign_lsn(uint cycle, uint block)
{
	return ((xfs_lsn_t)cycle << 32) | block;
}

static inline uint xlog_get_cycle(char *ptr)
{
	if (be32_to_cpu(*(__be32 *)ptr) == XLOG_HEADER_MAGIC_NUM)
		return be32_to_cpu(*((__be32 *)ptr + 1));
	else
		return be32_to_cpu(*(__be32 *)ptr);
}


#define XFS_TRANSACTION		0x69
#define XFS_LOG			0xaa

#define XLOG_UNMOUNT_TYPE	0x556e	


struct xfs_unmount_log_format {
	uint16_t	magic;	
	uint16_t	pad1;
	uint32_t	pad2;	
};


#define XLOG_REG_TYPE_BFORMAT		1
#define XLOG_REG_TYPE_BCHUNK		2
#define XLOG_REG_TYPE_EFI_FORMAT	3
#define XLOG_REG_TYPE_EFD_FORMAT	4
#define XLOG_REG_TYPE_IFORMAT		5
#define XLOG_REG_TYPE_ICORE		6
#define XLOG_REG_TYPE_IEXT		7
#define XLOG_REG_TYPE_IBROOT		8
#define XLOG_REG_TYPE_ILOCAL		9
#define XLOG_REG_TYPE_IATTR_EXT		10
#define XLOG_REG_TYPE_IATTR_BROOT	11
#define XLOG_REG_TYPE_IATTR_LOCAL	12
#define XLOG_REG_TYPE_QFORMAT		13
#define XLOG_REG_TYPE_DQUOT		14
#define XLOG_REG_TYPE_QUOTAOFF		15
#define XLOG_REG_TYPE_LRHEADER		16
#define XLOG_REG_TYPE_UNMOUNT		17
#define XLOG_REG_TYPE_COMMIT		18
#define XLOG_REG_TYPE_TRANSHDR		19
#define XLOG_REG_TYPE_ICREATE		20
#define XLOG_REG_TYPE_RUI_FORMAT	21
#define XLOG_REG_TYPE_RUD_FORMAT	22
#define XLOG_REG_TYPE_CUI_FORMAT	23
#define XLOG_REG_TYPE_CUD_FORMAT	24
#define XLOG_REG_TYPE_BUI_FORMAT	25
#define XLOG_REG_TYPE_BUD_FORMAT	26
#define XLOG_REG_TYPE_ATTRI_FORMAT	27
#define XLOG_REG_TYPE_ATTRD_FORMAT	28
#define XLOG_REG_TYPE_ATTR_NAME	29
#define XLOG_REG_TYPE_ATTR_VALUE	30
#define XLOG_REG_TYPE_MAX		30



#define XLOG_START_TRANS	0x01	
#define XLOG_COMMIT_TRANS	0x02	
#define XLOG_CONTINUE_TRANS	0x04	
#define XLOG_WAS_CONT_TRANS	0x08	
#define XLOG_END_TRANS		0x10	
#define XLOG_UNMOUNT_TRANS	0x20	


typedef struct xlog_op_header {
	__be32	   oh_tid;	
	__be32	   oh_len;	
	__u8	   oh_clientid;	
	__u8	   oh_flags;	
	__u16	   oh_res2;	
} xlog_op_header_t;


#define XLOG_FMT_UNKNOWN  0
#define XLOG_FMT_LINUX_LE 1
#define XLOG_FMT_LINUX_BE 2
#define XLOG_FMT_IRIX_BE  3


#ifdef XFS_NATIVE_HOST
#define XLOG_FMT XLOG_FMT_LINUX_BE
#else
#define XLOG_FMT XLOG_FMT_LINUX_LE
#endif

typedef struct xlog_rec_header {
	__be32	  h_magicno;	
	__be32	  h_cycle;	
	__be32	  h_version;	
	__be32	  h_len;	
	__be64	  h_lsn;	
	__be64	  h_tail_lsn;	
	__le32	  h_crc;	
	__be32	  h_prev_block; 
	__be32	  h_num_logops;	
	__be32	  h_cycle_data[XLOG_HEADER_CYCLE_SIZE / BBSIZE];
	
	__be32    h_fmt;        
	uuid_t	  h_fs_uuid;    
	__be32	  h_size;	
} xlog_rec_header_t;

typedef struct xlog_rec_ext_header {
	__be32	  xh_cycle;	
	__be32	  xh_cycle_data[XLOG_HEADER_CYCLE_SIZE / BBSIZE]; 
} xlog_rec_ext_header_t;


typedef union xlog_in_core2 {
	xlog_rec_header_t	hic_header;
	xlog_rec_ext_header_t	hic_xheader;
	char			hic_sector[XLOG_HEADER_SIZE];
} xlog_in_core_2_t;


typedef struct xfs_log_iovec {
	void		*i_addr;	
	int		i_len;		
	uint		i_type;		
} xfs_log_iovec_t;



typedef struct xfs_trans_header {
	uint		th_magic;		
	uint		th_type;		
	int32_t		th_tid;			
	uint		th_num_items;		
} xfs_trans_header_t;

#define	XFS_TRANS_HEADER_MAGIC	0x5452414e	


#define XFS_TRANS_CHECKPOINT	40


#define	XFS_LI_EFI		0x1236
#define	XFS_LI_EFD		0x1237
#define	XFS_LI_IUNLINK		0x1238
#define	XFS_LI_INODE		0x123b	
#define	XFS_LI_BUF		0x123c	
#define	XFS_LI_DQUOT		0x123d
#define	XFS_LI_QUOTAOFF		0x123e
#define	XFS_LI_ICREATE		0x123f
#define	XFS_LI_RUI		0x1240	
#define	XFS_LI_RUD		0x1241
#define	XFS_LI_CUI		0x1242	
#define	XFS_LI_CUD		0x1243
#define	XFS_LI_BUI		0x1244	
#define	XFS_LI_BUD		0x1245
#define	XFS_LI_ATTRI		0x1246  
#define	XFS_LI_ATTRD		0x1247  

#define XFS_LI_TYPE_DESC \
	{ XFS_LI_EFI,		"XFS_LI_EFI" }, \
	{ XFS_LI_EFD,		"XFS_LI_EFD" }, \
	{ XFS_LI_IUNLINK,	"XFS_LI_IUNLINK" }, \
	{ XFS_LI_INODE,		"XFS_LI_INODE" }, \
	{ XFS_LI_BUF,		"XFS_LI_BUF" }, \
	{ XFS_LI_DQUOT,		"XFS_LI_DQUOT" }, \
	{ XFS_LI_QUOTAOFF,	"XFS_LI_QUOTAOFF" }, \
	{ XFS_LI_ICREATE,	"XFS_LI_ICREATE" }, \
	{ XFS_LI_RUI,		"XFS_LI_RUI" }, \
	{ XFS_LI_RUD,		"XFS_LI_RUD" }, \
	{ XFS_LI_CUI,		"XFS_LI_CUI" }, \
	{ XFS_LI_CUD,		"XFS_LI_CUD" }, \
	{ XFS_LI_BUI,		"XFS_LI_BUI" }, \
	{ XFS_LI_BUD,		"XFS_LI_BUD" }, \
	{ XFS_LI_ATTRI,		"XFS_LI_ATTRI" }, \
	{ XFS_LI_ATTRD,		"XFS_LI_ATTRD" }


struct xfs_inode_log_format {
	uint16_t		ilf_type;	
	uint16_t		ilf_size;	
	uint32_t		ilf_fields;	
	uint16_t		ilf_asize;	
	uint16_t		ilf_dsize;	
	uint32_t		ilf_pad;	
	uint64_t		ilf_ino;	
	union {
		uint32_t	ilfu_rdev;	
		uint8_t		__pad[16];	
	} ilf_u;
	int64_t			ilf_blkno;	
	int32_t			ilf_len;	
	int32_t			ilf_boffset;	
};


struct xfs_inode_log_format_32 {
	uint16_t		ilf_type;	
	uint16_t		ilf_size;	
	uint32_t		ilf_fields;	
	uint16_t		ilf_asize;	
	uint16_t		ilf_dsize;	
	uint64_t		ilf_ino;	
	union {
		uint32_t	ilfu_rdev;	
		uint8_t		__pad[16];	
	} ilf_u;
	int64_t			ilf_blkno;	
	int32_t			ilf_len;	
	int32_t			ilf_boffset;	
} __attribute__((packed));



#define	XFS_ILOG_CORE	0x001	
#define	XFS_ILOG_DDATA	0x002	
#define	XFS_ILOG_DEXT	0x004	
#define	XFS_ILOG_DBROOT	0x008	
#define	XFS_ILOG_DEV	0x010	
#define	XFS_ILOG_UUID	0x020	
#define	XFS_ILOG_ADATA	0x040	
#define	XFS_ILOG_AEXT	0x080	
#define	XFS_ILOG_ABROOT	0x100	
#define XFS_ILOG_DOWNER	0x200	
#define XFS_ILOG_AOWNER	0x400	


#define XFS_ILOG_TIMESTAMP	0x4000


#define XFS_ILOG_IVERSION	0x8000

#define	XFS_ILOG_NONCORE	(XFS_ILOG_DDATA | XFS_ILOG_DEXT | \
				 XFS_ILOG_DBROOT | XFS_ILOG_DEV | \
				 XFS_ILOG_ADATA | XFS_ILOG_AEXT | \
				 XFS_ILOG_ABROOT | XFS_ILOG_DOWNER | \
				 XFS_ILOG_AOWNER)

#define	XFS_ILOG_DFORK		(XFS_ILOG_DDATA | XFS_ILOG_DEXT | \
				 XFS_ILOG_DBROOT)

#define	XFS_ILOG_AFORK		(XFS_ILOG_ADATA | XFS_ILOG_AEXT | \
				 XFS_ILOG_ABROOT)

#define	XFS_ILOG_ALL		(XFS_ILOG_CORE | XFS_ILOG_DDATA | \
				 XFS_ILOG_DEXT | XFS_ILOG_DBROOT | \
				 XFS_ILOG_DEV | XFS_ILOG_ADATA | \
				 XFS_ILOG_AEXT | XFS_ILOG_ABROOT | \
				 XFS_ILOG_TIMESTAMP | XFS_ILOG_DOWNER | \
				 XFS_ILOG_AOWNER)

static inline int xfs_ilog_fbroot(int w)
{
	return (w == XFS_DATA_FORK ? XFS_ILOG_DBROOT : XFS_ILOG_ABROOT);
}

static inline int xfs_ilog_fext(int w)
{
	return (w == XFS_DATA_FORK ? XFS_ILOG_DEXT : XFS_ILOG_AEXT);
}

static inline int xfs_ilog_fdata(int w)
{
	return (w == XFS_DATA_FORK ? XFS_ILOG_DDATA : XFS_ILOG_ADATA);
}


typedef uint64_t xfs_log_timestamp_t;


struct xfs_log_legacy_timestamp {
	int32_t		t_sec;		
	int32_t		t_nsec;		
};


struct xfs_log_dinode {
	uint16_t	di_magic;	
	uint16_t	di_mode;	
	int8_t		di_version;	
	int8_t		di_format;	
	uint8_t		di_pad3[2];	
	uint32_t	di_uid;		
	uint32_t	di_gid;		
	uint32_t	di_nlink;	
	uint16_t	di_projid_lo;	
	uint16_t	di_projid_hi;	
	union {
		
		uint64_t	di_big_nextents;

		
		uint64_t	di_v3_pad;

		
		struct {
			uint8_t	di_v2_pad[6];	
			uint16_t di_flushiter;	
		};
	};
	xfs_log_timestamp_t di_atime;	
	xfs_log_timestamp_t di_mtime;	
	xfs_log_timestamp_t di_ctime;	
	xfs_fsize_t	di_size;	
	xfs_rfsblock_t	di_nblocks;	
	xfs_extlen_t	di_extsize;	
	union {
		
		struct {
			uint32_t  di_nextents;
			uint16_t  di_anextents;
		} __packed;

		
		struct {
			uint32_t  di_big_anextents;
			uint16_t  di_nrext64_pad;
		} __packed;
	} __packed;
	uint8_t		di_forkoff;	
	int8_t		di_aformat;	
	uint32_t	di_dmevmask;	
	uint16_t	di_dmstate;	
	uint16_t	di_flags;	
	uint32_t	di_gen;		

	
	xfs_agino_t	di_next_unlinked;

	
	uint32_t	di_crc;		
	uint64_t	di_changecount;	

	
	xfs_lsn_t	di_lsn;

	uint64_t	di_flags2;	
	uint32_t	di_cowextsize;	
	uint8_t		di_pad2[12];	

	
	xfs_log_timestamp_t di_crtime;	
	xfs_ino_t	di_ino;		
	uuid_t		di_uuid;	

	
};

#define xfs_log_dinode_size(mp)						\
	(xfs_has_v3inodes((mp)) ?					\
		sizeof(struct xfs_log_dinode) :				\
		offsetof(struct xfs_log_dinode, di_next_unlinked))


#define	XFS_BLF_CHUNK		128
#define	XFS_BLF_SHIFT		7
#define	BIT_TO_WORD_SHIFT	5
#define	NBWORD			(NBBY * sizeof(unsigned int))


#define	XFS_BLF_INODE_BUF	(1<<0)


#define	XFS_BLF_CANCEL		(1<<1)


#define	XFS_BLF_UDQUOT_BUF	(1<<2)
#define XFS_BLF_PDQUOT_BUF	(1<<3)
#define	XFS_BLF_GDQUOT_BUF	(1<<4)


#define __XFS_BLF_DATAMAP_SIZE	((XFS_MAX_BLOCKSIZE / XFS_BLF_CHUNK) / NBWORD)
#define XFS_BLF_DATAMAP_SIZE	(__XFS_BLF_DATAMAP_SIZE + 1)

typedef struct xfs_buf_log_format {
	unsigned short	blf_type;	
	unsigned short	blf_size;	
	unsigned short	blf_flags;	
	unsigned short	blf_len;	
	int64_t		blf_blkno;	
	unsigned int	blf_map_size;	
	unsigned int	blf_data_map[XFS_BLF_DATAMAP_SIZE]; 
} xfs_buf_log_format_t;


#define XFS_BLFT_BITS	5
#define XFS_BLFT_SHIFT	11
#define XFS_BLFT_MASK	(((1 << XFS_BLFT_BITS) - 1) << XFS_BLFT_SHIFT)

enum xfs_blft {
	XFS_BLFT_UNKNOWN_BUF = 0,
	XFS_BLFT_UDQUOT_BUF,
	XFS_BLFT_PDQUOT_BUF,
	XFS_BLFT_GDQUOT_BUF,
	XFS_BLFT_BTREE_BUF,
	XFS_BLFT_AGF_BUF,
	XFS_BLFT_AGFL_BUF,
	XFS_BLFT_AGI_BUF,
	XFS_BLFT_DINO_BUF,
	XFS_BLFT_SYMLINK_BUF,
	XFS_BLFT_DIR_BLOCK_BUF,
	XFS_BLFT_DIR_DATA_BUF,
	XFS_BLFT_DIR_FREE_BUF,
	XFS_BLFT_DIR_LEAF1_BUF,
	XFS_BLFT_DIR_LEAFN_BUF,
	XFS_BLFT_DA_NODE_BUF,
	XFS_BLFT_ATTR_LEAF_BUF,
	XFS_BLFT_ATTR_RMT_BUF,
	XFS_BLFT_SB_BUF,
	XFS_BLFT_RTBITMAP_BUF,
	XFS_BLFT_RTSUMMARY_BUF,
	XFS_BLFT_MAX_BUF = (1 << XFS_BLFT_BITS),
};

static inline void
xfs_blft_to_flags(struct xfs_buf_log_format *blf, enum xfs_blft type)
{
	ASSERT(type > XFS_BLFT_UNKNOWN_BUF && type < XFS_BLFT_MAX_BUF);
	blf->blf_flags &= ~XFS_BLFT_MASK;
	blf->blf_flags |= ((type << XFS_BLFT_SHIFT) & XFS_BLFT_MASK);
}

static inline uint16_t
xfs_blft_from_flags(struct xfs_buf_log_format *blf)
{
	return (blf->blf_flags & XFS_BLFT_MASK) >> XFS_BLFT_SHIFT;
}


typedef struct xfs_extent {
	xfs_fsblock_t	ext_start;
	xfs_extlen_t	ext_len;
} xfs_extent_t;


typedef struct xfs_extent_32 {
	uint64_t	ext_start;
	uint32_t	ext_len;
} __attribute__((packed)) xfs_extent_32_t;

typedef struct xfs_extent_64 {
	uint64_t	ext_start;
	uint32_t	ext_len;
	uint32_t	ext_pad;
} xfs_extent_64_t;


typedef struct xfs_efi_log_format {
	uint16_t		efi_type;	
	uint16_t		efi_size;	
	uint32_t		efi_nextents;	
	uint64_t		efi_id;		
	xfs_extent_t		efi_extents[];	
} xfs_efi_log_format_t;

static inline size_t
xfs_efi_log_format_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_efi_log_format) +
			nr * sizeof(struct xfs_extent);
}

typedef struct xfs_efi_log_format_32 {
	uint16_t		efi_type;	
	uint16_t		efi_size;	
	uint32_t		efi_nextents;	
	uint64_t		efi_id;		
	xfs_extent_32_t		efi_extents[];	
} __attribute__((packed)) xfs_efi_log_format_32_t;

static inline size_t
xfs_efi_log_format32_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_efi_log_format_32) +
			nr * sizeof(struct xfs_extent_32);
}

typedef struct xfs_efi_log_format_64 {
	uint16_t		efi_type;	
	uint16_t		efi_size;	
	uint32_t		efi_nextents;	
	uint64_t		efi_id;		
	xfs_extent_64_t		efi_extents[];	
} xfs_efi_log_format_64_t;

static inline size_t
xfs_efi_log_format64_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_efi_log_format_64) +
			nr * sizeof(struct xfs_extent_64);
}


typedef struct xfs_efd_log_format {
	uint16_t		efd_type;	
	uint16_t		efd_size;	
	uint32_t		efd_nextents;	
	uint64_t		efd_efi_id;	
	xfs_extent_t		efd_extents[];	
} xfs_efd_log_format_t;

static inline size_t
xfs_efd_log_format_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_efd_log_format) +
			nr * sizeof(struct xfs_extent);
}

typedef struct xfs_efd_log_format_32 {
	uint16_t		efd_type;	
	uint16_t		efd_size;	
	uint32_t		efd_nextents;	
	uint64_t		efd_efi_id;	
	xfs_extent_32_t		efd_extents[];	
} __attribute__((packed)) xfs_efd_log_format_32_t;

static inline size_t
xfs_efd_log_format32_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_efd_log_format_32) +
			nr * sizeof(struct xfs_extent_32);
}

typedef struct xfs_efd_log_format_64 {
	uint16_t		efd_type;	
	uint16_t		efd_size;	
	uint32_t		efd_nextents;	
	uint64_t		efd_efi_id;	
	xfs_extent_64_t		efd_extents[];	
} xfs_efd_log_format_64_t;

static inline size_t
xfs_efd_log_format64_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_efd_log_format_64) +
			nr * sizeof(struct xfs_extent_64);
}


struct xfs_map_extent {
	uint64_t		me_owner;
	uint64_t		me_startblock;
	uint64_t		me_startoff;
	uint32_t		me_len;
	uint32_t		me_flags;
};


#define XFS_RMAP_EXTENT_MAP		1
#define XFS_RMAP_EXTENT_MAP_SHARED	2
#define XFS_RMAP_EXTENT_UNMAP		3
#define XFS_RMAP_EXTENT_UNMAP_SHARED	4
#define XFS_RMAP_EXTENT_CONVERT		5
#define XFS_RMAP_EXTENT_CONVERT_SHARED	6
#define XFS_RMAP_EXTENT_ALLOC		7
#define XFS_RMAP_EXTENT_FREE		8
#define XFS_RMAP_EXTENT_TYPE_MASK	0xFF

#define XFS_RMAP_EXTENT_ATTR_FORK	(1U << 31)
#define XFS_RMAP_EXTENT_BMBT_BLOCK	(1U << 30)
#define XFS_RMAP_EXTENT_UNWRITTEN	(1U << 29)

#define XFS_RMAP_EXTENT_FLAGS		(XFS_RMAP_EXTENT_TYPE_MASK | \
					 XFS_RMAP_EXTENT_ATTR_FORK | \
					 XFS_RMAP_EXTENT_BMBT_BLOCK | \
					 XFS_RMAP_EXTENT_UNWRITTEN)


struct xfs_rui_log_format {
	uint16_t		rui_type;	
	uint16_t		rui_size;	
	uint32_t		rui_nextents;	
	uint64_t		rui_id;		
	struct xfs_map_extent	rui_extents[];	
};

static inline size_t
xfs_rui_log_format_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_rui_log_format) +
			nr * sizeof(struct xfs_map_extent);
}


struct xfs_rud_log_format {
	uint16_t		rud_type;	
	uint16_t		rud_size;	
	uint32_t		__pad;
	uint64_t		rud_rui_id;	
};


struct xfs_phys_extent {
	uint64_t		pe_startblock;
	uint32_t		pe_len;
	uint32_t		pe_flags;
};



#define XFS_REFCOUNT_EXTENT_TYPE_MASK	0xFF

#define XFS_REFCOUNT_EXTENT_FLAGS	(XFS_REFCOUNT_EXTENT_TYPE_MASK)


struct xfs_cui_log_format {
	uint16_t		cui_type;	
	uint16_t		cui_size;	
	uint32_t		cui_nextents;	
	uint64_t		cui_id;		
	struct xfs_phys_extent	cui_extents[];	
};

static inline size_t
xfs_cui_log_format_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_cui_log_format) +
			nr * sizeof(struct xfs_phys_extent);
}


struct xfs_cud_log_format {
	uint16_t		cud_type;	
	uint16_t		cud_size;	
	uint32_t		__pad;
	uint64_t		cud_cui_id;	
};





#define XFS_BMAP_EXTENT_TYPE_MASK	0xFF

#define XFS_BMAP_EXTENT_ATTR_FORK	(1U << 31)
#define XFS_BMAP_EXTENT_UNWRITTEN	(1U << 30)

#define XFS_BMAP_EXTENT_FLAGS		(XFS_BMAP_EXTENT_TYPE_MASK | \
					 XFS_BMAP_EXTENT_ATTR_FORK | \
					 XFS_BMAP_EXTENT_UNWRITTEN)


struct xfs_bui_log_format {
	uint16_t		bui_type;	
	uint16_t		bui_size;	
	uint32_t		bui_nextents;	
	uint64_t		bui_id;		
	struct xfs_map_extent	bui_extents[];	
};

static inline size_t
xfs_bui_log_format_sizeof(
	unsigned int		nr)
{
	return sizeof(struct xfs_bui_log_format) +
			nr * sizeof(struct xfs_map_extent);
}


struct xfs_bud_log_format {
	uint16_t		bud_type;	
	uint16_t		bud_size;	
	uint32_t		__pad;
	uint64_t		bud_bui_id;	
};


typedef struct xfs_dq_logformat {
	uint16_t		qlf_type;      
	uint16_t		qlf_size;      
	xfs_dqid_t		qlf_id;	       
	int64_t			qlf_blkno;     
	int32_t			qlf_len;       
	uint32_t		qlf_boffset;   
} xfs_dq_logformat_t;


typedef struct xfs_qoff_logformat {
	unsigned short		qf_type;	
	unsigned short		qf_size;	
	unsigned int		qf_flags;	
	char			qf_pad[12];	
} xfs_qoff_logformat_t;


#define XFS_UQUOTA_ACCT	0x0001  
#define XFS_UQUOTA_ENFD	0x0002  
#define XFS_UQUOTA_CHKD	0x0004  
#define XFS_PQUOTA_ACCT	0x0008  
#define XFS_OQUOTA_ENFD	0x0010  
#define XFS_OQUOTA_CHKD	0x0020  
#define XFS_GQUOTA_ACCT	0x0040  


#define XFS_GQUOTA_ENFD	0x0080  
#define XFS_GQUOTA_CHKD	0x0100  
#define XFS_PQUOTA_ENFD	0x0200  
#define XFS_PQUOTA_CHKD	0x0400  

#define XFS_ALL_QUOTA_ACCT	\
		(XFS_UQUOTA_ACCT | XFS_GQUOTA_ACCT | XFS_PQUOTA_ACCT)
#define XFS_ALL_QUOTA_ENFD	\
		(XFS_UQUOTA_ENFD | XFS_GQUOTA_ENFD | XFS_PQUOTA_ENFD)
#define XFS_ALL_QUOTA_CHKD	\
		(XFS_UQUOTA_CHKD | XFS_GQUOTA_CHKD | XFS_PQUOTA_CHKD)

#define XFS_MOUNT_QUOTA_ALL	(XFS_UQUOTA_ACCT|XFS_UQUOTA_ENFD|\
				 XFS_UQUOTA_CHKD|XFS_GQUOTA_ACCT|\
				 XFS_GQUOTA_ENFD|XFS_GQUOTA_CHKD|\
				 XFS_PQUOTA_ACCT|XFS_PQUOTA_ENFD|\
				 XFS_PQUOTA_CHKD)


struct xfs_icreate_log {
	uint16_t	icl_type;	
	uint16_t	icl_size;	
	__be32		icl_ag;		
	__be32		icl_agbno;	
	__be32		icl_count;	
	__be32		icl_isize;	
	__be32		icl_length;	
	__be32		icl_gen;	
};


#define XFS_ATTRI_OP_FLAGS_SET		1	
#define XFS_ATTRI_OP_FLAGS_REMOVE	2	
#define XFS_ATTRI_OP_FLAGS_REPLACE	3	
#define XFS_ATTRI_OP_FLAGS_TYPE_MASK	0xFF	


#define XFS_ATTRI_FILTER_MASK		(XFS_ATTR_ROOT | \
					 XFS_ATTR_SECURE | \
					 XFS_ATTR_INCOMPLETE)


struct xfs_attri_log_format {
	uint16_t	alfi_type;	
	uint16_t	alfi_size;	
	uint32_t	__pad;		
	uint64_t	alfi_id;	
	uint64_t	alfi_ino;	
	uint32_t	alfi_op_flags;	
	uint32_t	alfi_name_len;	
	uint32_t	alfi_value_len;	
	uint32_t	alfi_attr_filter;
};

struct xfs_attrd_log_format {
	uint16_t	alfd_type;	
	uint16_t	alfd_size;	
	uint32_t	__pad;		
	uint64_t	alfd_alf_id;	
};

#endif 
