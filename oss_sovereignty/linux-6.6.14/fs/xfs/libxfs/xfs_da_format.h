#ifndef __XFS_DA_FORMAT_H__
#define __XFS_DA_FORMAT_H__
#define XFS_DA_NODE_MAGIC	0xfebe	 
#define XFS_ATTR_LEAF_MAGIC	0xfbee	 
#define XFS_DIR2_LEAF1_MAGIC	0xd2f1	 
#define XFS_DIR2_LEAFN_MAGIC	0xd2ff	 
typedef struct xfs_da_blkinfo {
	__be32		forw;			 
	__be32		back;			 
	__be16		magic;			 
	__be16		pad;			 
} xfs_da_blkinfo_t;
#define XFS_DA3_NODE_MAGIC	0x3ebe	 
#define XFS_ATTR3_LEAF_MAGIC	0x3bee	 
#define XFS_DIR3_LEAF1_MAGIC	0x3df1	 
#define XFS_DIR3_LEAFN_MAGIC	0x3dff	 
struct xfs_da3_blkinfo {
	struct xfs_da_blkinfo	hdr;
	__be32			crc;	 
	__be64			blkno;	 
	__be64			lsn;	 
	uuid_t			uuid;	 
	__be64			owner;	 
};
#define XFS_DA_NODE_MAXDEPTH	5	 
typedef struct xfs_da_node_hdr {
	struct xfs_da_blkinfo	info;	 
	__be16			__count;  
	__be16			__level;  
} xfs_da_node_hdr_t;
struct xfs_da3_node_hdr {
	struct xfs_da3_blkinfo	info;	 
	__be16			__count;  
	__be16			__level;  
	__be32			__pad32;
};
#define XFS_DA3_NODE_CRC_OFF	(offsetof(struct xfs_da3_node_hdr, info.crc))
typedef struct xfs_da_node_entry {
	__be32	hashval;	 
	__be32	before;		 
} xfs_da_node_entry_t;
typedef struct xfs_da_intnode {
	struct xfs_da_node_hdr	hdr;
	struct xfs_da_node_entry __btree[];
} xfs_da_intnode_t;
struct xfs_da3_intnode {
	struct xfs_da3_node_hdr	hdr;
	struct xfs_da_node_entry __btree[];
};
#define	XFS_DIR2_BLOCK_MAGIC	0x58443242	 
#define	XFS_DIR2_DATA_MAGIC	0x58443244	 
#define	XFS_DIR2_FREE_MAGIC	0x58443246	 
#define	XFS_DIR3_BLOCK_MAGIC	0x58444233	 
#define	XFS_DIR3_DATA_MAGIC	0x58444433	 
#define	XFS_DIR3_FREE_MAGIC	0x58444633	 
#define XFS_DIR3_FT_UNKNOWN		0
#define XFS_DIR3_FT_REG_FILE		1
#define XFS_DIR3_FT_DIR			2
#define XFS_DIR3_FT_CHRDEV		3
#define XFS_DIR3_FT_BLKDEV		4
#define XFS_DIR3_FT_FIFO		5
#define XFS_DIR3_FT_SOCK		6
#define XFS_DIR3_FT_SYMLINK		7
#define XFS_DIR3_FT_WHT			8
#define XFS_DIR3_FT_MAX			9
typedef uint16_t	xfs_dir2_data_off_t;
#define	NULLDATAOFF	0xffffU
typedef uint		xfs_dir2_data_aoff_t;	 
typedef uint32_t	xfs_dir2_dataptr_t;
#define	XFS_DIR2_MAX_DATAPTR	((xfs_dir2_dataptr_t)0xffffffff)
#define	XFS_DIR2_NULL_DATAPTR	((xfs_dir2_dataptr_t)0)
typedef	xfs_off_t	xfs_dir2_off_t;
typedef uint32_t	xfs_dir2_db_t;
#define XFS_INO32_SIZE	4
#define XFS_INO64_SIZE	8
#define XFS_INO64_DIFF	(XFS_INO64_SIZE - XFS_INO32_SIZE)
#define	XFS_DIR2_MAX_SHORT_INUM	((xfs_ino_t)0xffffffffULL)
typedef struct xfs_dir2_sf_hdr {
	uint8_t			count;		 
	uint8_t			i8count;	 
	uint8_t			parent[8];	 
} __packed xfs_dir2_sf_hdr_t;
typedef struct xfs_dir2_sf_entry {
	__u8			namelen;	 
	__u8			offset[2];	 
	__u8			name[];		 
} __packed xfs_dir2_sf_entry_t;
static inline int xfs_dir2_sf_hdr_size(int i8count)
{
	return sizeof(struct xfs_dir2_sf_hdr) -
		(i8count == 0) * XFS_INO64_DIFF;
}
static inline xfs_dir2_data_aoff_t
xfs_dir2_sf_get_offset(xfs_dir2_sf_entry_t *sfep)
{
	return get_unaligned_be16(sfep->offset);
}
static inline void
xfs_dir2_sf_put_offset(xfs_dir2_sf_entry_t *sfep, xfs_dir2_data_aoff_t off)
{
	put_unaligned_be16(off, sfep->offset);
}
static inline struct xfs_dir2_sf_entry *
xfs_dir2_sf_firstentry(struct xfs_dir2_sf_hdr *hdr)
{
	return (struct xfs_dir2_sf_entry *)
		((char *)hdr + xfs_dir2_sf_hdr_size(hdr->i8count));
}
#define	XFS_DIR2_DATA_ALIGN_LOG	3		 
#define	XFS_DIR2_DATA_ALIGN	(1 << XFS_DIR2_DATA_ALIGN_LOG)
#define	XFS_DIR2_DATA_FREE_TAG	0xffff
#define	XFS_DIR2_DATA_FD_COUNT	3
#define	XFS_DIR2_MAX_SPACES	3
#define	XFS_DIR2_SPACE_SIZE	(1ULL << (32 + XFS_DIR2_DATA_ALIGN_LOG))
#define	XFS_DIR2_DATA_SPACE	0
#define	XFS_DIR2_DATA_OFFSET	(XFS_DIR2_DATA_SPACE * XFS_DIR2_SPACE_SIZE)
typedef struct xfs_dir2_data_free {
	__be16			offset;		 
	__be16			length;		 
} xfs_dir2_data_free_t;
typedef struct xfs_dir2_data_hdr {
	__be32			magic;		 
	xfs_dir2_data_free_t	bestfree[XFS_DIR2_DATA_FD_COUNT];
} xfs_dir2_data_hdr_t;
struct xfs_dir3_blk_hdr {
	__be32			magic;	 
	__be32			crc;	 
	__be64			blkno;	 
	__be64			lsn;	 
	uuid_t			uuid;	 
	__be64			owner;	 
};
struct xfs_dir3_data_hdr {
	struct xfs_dir3_blk_hdr	hdr;
	xfs_dir2_data_free_t	best_free[XFS_DIR2_DATA_FD_COUNT];
	__be32			pad;	 
};
#define XFS_DIR3_DATA_CRC_OFF  offsetof(struct xfs_dir3_data_hdr, hdr.crc)
typedef struct xfs_dir2_data_entry {
	__be64			inumber;	 
	__u8			namelen;	 
	__u8			name[];		 
} xfs_dir2_data_entry_t;
typedef struct xfs_dir2_data_unused {
	__be16			freetag;	 
	__be16			length;		 
	__be16			tag;		 
} xfs_dir2_data_unused_t;
static inline __be16 *
xfs_dir2_data_unused_tag_p(struct xfs_dir2_data_unused *dup)
{
	return (__be16 *)((char *)dup +
			be16_to_cpu(dup->length) - sizeof(__be16));
}
#define	XFS_DIR2_LEAF_SPACE	1
#define	XFS_DIR2_LEAF_OFFSET	(XFS_DIR2_LEAF_SPACE * XFS_DIR2_SPACE_SIZE)
typedef struct xfs_dir2_leaf_hdr {
	xfs_da_blkinfo_t	info;		 
	__be16			count;		 
	__be16			stale;		 
} xfs_dir2_leaf_hdr_t;
struct xfs_dir3_leaf_hdr {
	struct xfs_da3_blkinfo	info;		 
	__be16			count;		 
	__be16			stale;		 
	__be32			pad;		 
};
typedef struct xfs_dir2_leaf_entry {
	__be32			hashval;	 
	__be32			address;	 
} xfs_dir2_leaf_entry_t;
typedef struct xfs_dir2_leaf_tail {
	__be32			bestcount;
} xfs_dir2_leaf_tail_t;
typedef struct xfs_dir2_leaf {
	xfs_dir2_leaf_hdr_t	hdr;			 
	xfs_dir2_leaf_entry_t	__ents[];		 
} xfs_dir2_leaf_t;
struct xfs_dir3_leaf {
	struct xfs_dir3_leaf_hdr	hdr;		 
	struct xfs_dir2_leaf_entry	__ents[];	 
};
#define XFS_DIR3_LEAF_CRC_OFF  offsetof(struct xfs_dir3_leaf_hdr, info.crc)
static inline __be16 *
xfs_dir2_leaf_bests_p(struct xfs_dir2_leaf_tail *ltp)
{
	return (__be16 *)ltp - be32_to_cpu(ltp->bestcount);
}
#define	XFS_DIR2_FREE_SPACE	2
#define	XFS_DIR2_FREE_OFFSET	(XFS_DIR2_FREE_SPACE * XFS_DIR2_SPACE_SIZE)
typedef	struct xfs_dir2_free_hdr {
	__be32			magic;		 
	__be32			firstdb;	 
	__be32			nvalid;		 
	__be32			nused;		 
} xfs_dir2_free_hdr_t;
typedef struct xfs_dir2_free {
	xfs_dir2_free_hdr_t	hdr;		 
	__be16			bests[];	 
} xfs_dir2_free_t;
struct xfs_dir3_free_hdr {
	struct xfs_dir3_blk_hdr	hdr;
	__be32			firstdb;	 
	__be32			nvalid;		 
	__be32			nused;		 
	__be32			pad;		 
};
struct xfs_dir3_free {
	struct xfs_dir3_free_hdr hdr;
	__be16			bests[];	 
};
#define XFS_DIR3_FREE_CRC_OFF  offsetof(struct xfs_dir3_free, hdr.hdr.crc)
typedef struct xfs_dir2_block_tail {
	__be32		count;			 
	__be32		stale;			 
} xfs_dir2_block_tail_t;
static inline struct xfs_dir2_leaf_entry *
xfs_dir2_block_leaf_p(struct xfs_dir2_block_tail *btp)
{
	return ((struct xfs_dir2_leaf_entry *)btp) - be32_to_cpu(btp->count);
}
#define XFS_ATTR_LEAF_MAPSIZE	3	 
struct xfs_attr_shortform {
	struct xfs_attr_sf_hdr {	 
		__be16	totsize;	 
		__u8	count;	 
		__u8	padding;
	} hdr;
	struct xfs_attr_sf_entry {
		uint8_t namelen;	 
		uint8_t valuelen;	 
		uint8_t flags;	 
		uint8_t nameval[];	 
	} list[];			 
};
typedef struct xfs_attr_leaf_map {	 
	__be16	base;			   
	__be16	size;			   
} xfs_attr_leaf_map_t;
typedef struct xfs_attr_leaf_hdr {	 
	xfs_da_blkinfo_t info;		 
	__be16	count;			 
	__be16	usedbytes;		 
	__be16	firstused;		 
	__u8	holes;			 
	__u8	pad1;
	xfs_attr_leaf_map_t freemap[XFS_ATTR_LEAF_MAPSIZE];
} xfs_attr_leaf_hdr_t;
typedef struct xfs_attr_leaf_entry {	 
	__be32	hashval;		 
	__be16	nameidx;		 
	__u8	flags;			 
	__u8	pad2;			 
} xfs_attr_leaf_entry_t;
typedef struct xfs_attr_leaf_name_local {
	__be16	valuelen;		 
	__u8	namelen;		 
	__u8	nameval[];		 
} xfs_attr_leaf_name_local_t;
typedef struct xfs_attr_leaf_name_remote {
	__be32	valueblk;		 
	__be32	valuelen;		 
	__u8	namelen;		 
	__u8	name[];			 
} xfs_attr_leaf_name_remote_t;
typedef struct xfs_attr_leafblock {
	xfs_attr_leaf_hdr_t	hdr;	 
	xfs_attr_leaf_entry_t	entries[];	 
} xfs_attr_leafblock_t;
struct xfs_attr3_leaf_hdr {
	struct xfs_da3_blkinfo	info;
	__be16			count;
	__be16			usedbytes;
	__be16			firstused;
	__u8			holes;
	__u8			pad1;
	struct xfs_attr_leaf_map freemap[XFS_ATTR_LEAF_MAPSIZE];
	__be32			pad2;		 
};
#define XFS_ATTR3_LEAF_CRC_OFF	(offsetof(struct xfs_attr3_leaf_hdr, info.crc))
struct xfs_attr3_leafblock {
	struct xfs_attr3_leaf_hdr	hdr;
	struct xfs_attr_leaf_entry	entries[];
};
#define XFS_ATTR3_LEAF_NULLOFF	0
#define	XFS_ATTR_LOCAL_BIT	0	 
#define	XFS_ATTR_ROOT_BIT	1	 
#define	XFS_ATTR_SECURE_BIT	2	 
#define	XFS_ATTR_INCOMPLETE_BIT	7	 
#define XFS_ATTR_LOCAL		(1u << XFS_ATTR_LOCAL_BIT)
#define XFS_ATTR_ROOT		(1u << XFS_ATTR_ROOT_BIT)
#define XFS_ATTR_SECURE		(1u << XFS_ATTR_SECURE_BIT)
#define XFS_ATTR_INCOMPLETE	(1u << XFS_ATTR_INCOMPLETE_BIT)
#define XFS_ATTR_NSP_ONDISK_MASK	(XFS_ATTR_ROOT | XFS_ATTR_SECURE)
#define	XFS_ATTR_LEAF_NAME_ALIGN	((uint)sizeof(xfs_dablk_t))
static inline int
xfs_attr3_leaf_hdr_size(struct xfs_attr_leafblock *leafp)
{
	if (leafp->hdr.info.magic == cpu_to_be16(XFS_ATTR3_LEAF_MAGIC))
		return sizeof(struct xfs_attr3_leaf_hdr);
	return sizeof(struct xfs_attr_leaf_hdr);
}
static inline struct xfs_attr_leaf_entry *
xfs_attr3_leaf_entryp(xfs_attr_leafblock_t *leafp)
{
	if (leafp->hdr.info.magic == cpu_to_be16(XFS_ATTR3_LEAF_MAGIC))
		return &((struct xfs_attr3_leafblock *)leafp)->entries[0];
	return &leafp->entries[0];
}
static inline char *
xfs_attr3_leaf_name(xfs_attr_leafblock_t *leafp, int idx)
{
	struct xfs_attr_leaf_entry *entries = xfs_attr3_leaf_entryp(leafp);
	return &((char *)leafp)[be16_to_cpu(entries[idx].nameidx)];
}
static inline xfs_attr_leaf_name_remote_t *
xfs_attr3_leaf_name_remote(xfs_attr_leafblock_t *leafp, int idx)
{
	return (xfs_attr_leaf_name_remote_t *)xfs_attr3_leaf_name(leafp, idx);
}
static inline xfs_attr_leaf_name_local_t *
xfs_attr3_leaf_name_local(xfs_attr_leafblock_t *leafp, int idx)
{
	return (xfs_attr_leaf_name_local_t *)xfs_attr3_leaf_name(leafp, idx);
}
static inline int xfs_attr_leaf_entsize_remote(int nlen)
{
	const size_t remotesize =
			offsetof(struct xfs_attr_leaf_name_remote, name) + 2;
	return round_up(remotesize + nlen, XFS_ATTR_LEAF_NAME_ALIGN);
}
static inline int xfs_attr_leaf_entsize_local(int nlen, int vlen)
{
	const size_t localsize =
			offsetof(struct xfs_attr_leaf_name_local, nameval);
	return round_up(localsize + nlen + vlen, XFS_ATTR_LEAF_NAME_ALIGN);
}
static inline int xfs_attr_leaf_entsize_local_max(int bsize)
{
	return (((bsize) >> 1) + ((bsize) >> 2));
}
#define XFS_ATTR3_RMT_MAGIC	0x5841524d	 
struct xfs_attr3_rmt_hdr {
	__be32	rm_magic;
	__be32	rm_offset;
	__be32	rm_bytes;
	__be32	rm_crc;
	uuid_t	rm_uuid;
	__be64	rm_owner;
	__be64	rm_blkno;
	__be64	rm_lsn;
};
#define XFS_ATTR3_RMT_CRC_OFF	offsetof(struct xfs_attr3_rmt_hdr, rm_crc)
#define XFS_ATTR3_RMT_BUF_SPACE(mp, bufsize)	\
	((bufsize) - (xfs_has_crc((mp)) ? \
			sizeof(struct xfs_attr3_rmt_hdr) : 0))
static inline unsigned int xfs_dir2_dirblock_bytes(struct xfs_sb *sbp)
{
	return 1 << (sbp->sb_blocklog + sbp->sb_dirblklog);
}
xfs_failaddr_t xfs_da3_blkinfo_verify(struct xfs_buf *bp,
				      struct xfs_da3_blkinfo *hdr3);
#endif  
