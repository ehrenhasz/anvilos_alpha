#ifndef LINUX_EXT2_FS_H
#define LINUX_EXT2_FS_H 1
#define EXT2_BAD_INO		 1	 
#define EXT2_ROOT_INO		 2	 
#define EXT2_ACL_IDX_INO	 3	 
#define EXT2_ACL_DATA_INO	 4	 
#define EXT2_BOOT_LOADER_INO	 5	 
#define EXT2_UNDEL_DIR_INO	 6	 
#define EXT2_RESIZE_INO		 7	 
#define EXT2_JOURNAL_INO	 8	 
#define EXT2_GOOD_OLD_FIRST_INO	11
#define EXT2_SUPER_MAGIC	0xEF53
#define EXT2_SB(sb)	(sb)
#define EXT2_LINK_MAX		32000
#define EXT2_MIN_BLOCK_LOG_SIZE		10	 
#define EXT2_MAX_BLOCK_LOG_SIZE		16	 
#define EXT2_MIN_BLOCK_SIZE	(1 << EXT2_MIN_BLOCK_LOG_SIZE)
#define EXT2_MAX_BLOCK_SIZE	(1 << EXT2_MAX_BLOCK_LOG_SIZE)
#define EXT2_BLOCK_SIZE(s)	(EXT2_MIN_BLOCK_SIZE << (s)->s_log_block_size)
#define EXT2_BLOCK_SIZE_BITS(s)	((s)->s_log_block_size + 10)
#define EXT2_INODE_SIZE(s)	(((s)->s_rev_level == EXT2_GOOD_OLD_REV) ? \
				 EXT2_GOOD_OLD_INODE_SIZE : (s)->s_inode_size)
#define EXT2_FIRST_INO(s)	(((s)->s_rev_level == EXT2_GOOD_OLD_REV) ? \
				 EXT2_GOOD_OLD_FIRST_INO : (s)->s_first_ino)
#define EXT2_ADDR_PER_BLOCK(s)	(EXT2_BLOCK_SIZE(s) / sizeof(uint32_t))
#define EXT2_MIN_FRAG_SIZE		EXT2_MIN_BLOCK_SIZE
#define EXT2_MAX_FRAG_SIZE		EXT2_MAX_BLOCK_SIZE
#define EXT2_MIN_FRAG_LOG_SIZE		EXT2_MIN_BLOCK_LOG_SIZE
#define EXT2_FRAG_SIZE(s)		(EXT2_MIN_FRAG_SIZE << (s)->s_log_frag_size)
#define EXT2_FRAGS_PER_BLOCK(s)		(EXT2_BLOCK_SIZE(s) / EXT2_FRAG_SIZE(s))
struct ext2_acl_header {	 
	uint32_t	aclh_size;
	uint32_t	aclh_file_count;
	uint32_t	aclh_acle_count;
	uint32_t	aclh_first_acle;
};
struct ext2_acl_entry {	 
	uint32_t	acle_size;
	uint16_t	acle_perms;	 
	uint16_t	acle_type;	 
	uint16_t	acle_tag;	 
	uint16_t	acle_pad1;
	uint32_t	acle_next;	 
};
struct ext2_group_desc {
	uint32_t	bg_block_bitmap;	 
	uint32_t	bg_inode_bitmap;	 
	uint32_t	bg_inode_table;		 
	uint16_t	bg_free_blocks_count;	 
	uint16_t	bg_free_inodes_count;	 
	uint16_t	bg_used_dirs_count;	 
	uint16_t	bg_pad;
	uint32_t	bg_reserved[3];
};
struct ext2_dx_root_info {
	uint32_t	reserved_zero;
	uint8_t		hash_version;  
	uint8_t		info_length;  
	uint8_t		indirect_levels;
	uint8_t		unused_flags;
};
#define EXT2_HASH_LEGACY	0
#define EXT2_HASH_HALF_MD4	1
#define EXT2_HASH_TEA		2
#define EXT2_HASH_FLAG_INCOMPAT	0x1
struct ext2_dx_entry {
	uint32_t hash;
	uint32_t block;
};
struct ext2_dx_countlimit {
	uint16_t limit;
	uint16_t count;
};
#define EXT2_BLOCKS_PER_GROUP(s)	(EXT2_SB(s)->s_blocks_per_group)
#define EXT2_INODES_PER_GROUP(s)	(EXT2_SB(s)->s_inodes_per_group)
#define EXT2_INODES_PER_BLOCK(s)	(EXT2_BLOCK_SIZE(s)/EXT2_INODE_SIZE(s))
#define EXT2_MAX_BLOCKS_PER_GROUP(s)	((1 << 16) - 8)
#define EXT2_MAX_INODES_PER_GROUP(s)	((1 << 16) - EXT2_INODES_PER_BLOCK(s))
#define EXT2_DESC_PER_BLOCK(s)		(EXT2_BLOCK_SIZE(s) / sizeof (struct ext2_group_desc))
#define EXT2_NDIR_BLOCKS		12
#define EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)
#define EXT2_SECRM_FL			0x00000001  
#define EXT2_UNRM_FL			0x00000002  
#define EXT2_COMPR_FL			0x00000004  
#define EXT2_SYNC_FL			0x00000008  
#define EXT2_IMMUTABLE_FL		0x00000010  
#define EXT2_APPEND_FL			0x00000020  
#define EXT2_NODUMP_FL			0x00000040  
#define EXT2_NOATIME_FL			0x00000080  
#define EXT2_DIRTY_FL			0x00000100
#define EXT2_COMPRBLK_FL		0x00000200  
#define EXT2_NOCOMPR_FL			0x00000400  
#define EXT2_ECOMPR_FL			0x00000800  
#define EXT2_BTREE_FL			0x00001000  
#define EXT2_INDEX_FL			0x00001000  
#define EXT2_IMAGIC_FL			0x00002000
#define EXT3_JOURNAL_DATA_FL		0x00004000  
#define EXT2_NOTAIL_FL			0x00008000  
#define EXT2_DIRSYNC_FL			0x00010000  
#define EXT2_TOPDIR_FL			0x00020000  
#define EXT2_EXTENT_FL			0x00080000  
#define EXT2_VERITY_FL			0x00100000
#define EXT2_NOCOW_FL			0x00800000  
#define EXT2_INLINE_DATA_FL		0x10000000
#define EXT2_PROJINHERIT_FL		0x20000000
#define EXT2_CASEFOLD_FL		0x40000000
#define EXT2_IOC_GETFLAGS		_IOR('f', 1, long)
#define EXT2_IOC_SETFLAGS		_IOW('f', 2, long)
#define EXT2_IOC_GETVERSION		_IOR('v', 1, long)
#define EXT2_IOC_SETVERSION		_IOW('v', 2, long)
struct ext2_fsxattr {
	uint32_t	fsx_xflags;	 
	uint32_t	fsx_extsize;	 
	uint32_t	fsx_nextents;	 
	uint32_t	fsx_projid;	 
	uint32_t	fsx_cowextsize;	 
	unsigned char	fsx_pad[8];
};
#define EXT2_IOC_FSGETXATTR		_IOR('X', 31, struct ext2_fsxattr)
#define EXT2_IOC_FSSETXATTR		_IOW('X', 32, struct ext2_fsxattr)
struct ext2_inode {
	uint16_t	i_mode;		 
	uint16_t	i_uid;		 
	uint32_t	i_size;		 
	uint32_t	i_atime;	 
	uint32_t	i_ctime;	 
	uint32_t	i_mtime;	 
	uint32_t	i_dtime;	 
	uint16_t	i_gid;		 
	uint16_t	i_links_count;	 
	uint32_t	i_blocks;	 
	uint32_t	i_flags;	 
	union {
		struct {
			uint32_t  l_i_reserved1;
		} linux1;
		struct {
			uint32_t  h_i_translator;
		} hurd1;
		struct {
			uint32_t  m_i_reserved1;
		} masix1;
	} osd1;				 
	uint32_t	i_block[EXT2_N_BLOCKS]; 
	uint32_t	i_generation;	 
	uint32_t	i_file_acl;	 
	uint32_t	i_dir_acl;	 
	uint32_t	i_faddr;	 
	union {
		struct {
			uint8_t		l_i_frag;	 
			uint8_t		l_i_fsize;	 
			uint16_t	i_pad1;
			uint16_t	l_i_uid_high;	 
			uint16_t	l_i_gid_high;	 
			uint32_t	l_i_reserved2;
		} linux2;
		struct {
			uint8_t		h_i_frag;	 
			uint8_t		h_i_fsize;	 
			uint16_t	h_i_mode_high;
			uint16_t	h_i_uid_high;
			uint16_t	h_i_gid_high;
			uint32_t	h_i_author;
		} hurd2;
		struct {
			uint8_t		m_i_frag;	 
			uint8_t		m_i_fsize;	 
			uint16_t	m_pad1;
			uint32_t	m_i_reserved2[2];
		} masix2;
	} osd2;				 
};
struct ext2_inode_large {
	uint16_t	i_mode;		 
	uint16_t	i_uid;		 
	uint32_t	i_size;		 
	uint32_t	i_atime;	 
	uint32_t	i_ctime;	 
	uint32_t	i_mtime;	 
	uint32_t	i_dtime;	 
	uint16_t	i_gid;		 
	uint16_t	i_links_count;	 
	uint32_t	i_blocks;	 
	uint32_t	i_flags;	 
	union {
		struct {
			uint32_t  l_i_reserved1;
		} linux1;
		struct {
			uint32_t  h_i_translator;
		} hurd1;
		struct {
			uint32_t  m_i_reserved1;
		} masix1;
	} osd1;				 
	uint32_t	i_block[EXT2_N_BLOCKS]; 
	uint32_t	i_generation;	 
	uint32_t	i_file_acl;	 
	uint32_t	i_dir_acl;	 
	uint32_t	i_faddr;	 
	union {
		struct {
			uint8_t		l_i_frag;	 
			uint8_t		l_i_fsize;	 
			uint16_t	i_pad1;
			uint16_t	l_i_uid_high;	 
			uint16_t	l_i_gid_high;	 
			uint32_t	l_i_reserved2;
		} linux2;
		struct {
			uint8_t		h_i_frag;	 
			uint8_t		h_i_fsize;	 
			uint16_t	h_i_mode_high;
			uint16_t	h_i_uid_high;
			uint16_t	h_i_gid_high;
			uint32_t	h_i_author;
		} hurd2;
		struct {
			uint8_t		m_i_frag;	 
			uint8_t		m_i_fsize;	 
			uint16_t	m_pad1;
			uint32_t	m_i_reserved2[2];
		} masix2;
	} osd2;				 
	uint16_t	i_extra_isize;
	uint16_t	i_pad1;
};
#define i_size_high	i_dir_acl
#define EXT2_VALID_FS			0x0001	 
#define EXT2_ERROR_FS			0x0002	 
#define EXT2_MOUNT_CHECK		0x0001	 
#define EXT2_MOUNT_GRPID		0x0004	 
#define EXT2_MOUNT_DEBUG		0x0008	 
#define EXT2_MOUNT_ERRORS_CONT		0x0010	 
#define EXT2_MOUNT_ERRORS_RO		0x0020	 
#define EXT2_MOUNT_ERRORS_PANIC		0x0040	 
#define EXT2_MOUNT_MINIX_DF		0x0080	 
#define EXT2_MOUNT_NO_UID32		0x0200   
#define clear_opt(o, opt)		o &= ~EXT2_MOUNT_##opt
#define set_opt(o, opt)			o |= EXT2_MOUNT_##opt
#define test_opt(sb, opt)		(EXT2_SB(sb)->s_mount_opt & \
					 EXT2_MOUNT_##opt)
#define EXT2_DFL_MAX_MNT_COUNT		20	 
#define EXT2_DFL_CHECKINTERVAL		0	 
#define EXT2_ERRORS_CONTINUE		1	 
#define EXT2_ERRORS_RO			2	 
#define EXT2_ERRORS_PANIC		3	 
#define EXT2_ERRORS_DEFAULT		EXT2_ERRORS_CONTINUE
struct ext2_super_block {
	uint32_t	s_inodes_count;		 
	uint32_t	s_blocks_count;		 
	uint32_t	s_r_blocks_count;	 
	uint32_t	s_free_blocks_count;	 
	uint32_t	s_free_inodes_count;	 
	uint32_t	s_first_data_block;	 
	uint32_t	s_log_block_size;	 
	int32_t		s_log_frag_size;	 
	uint32_t	s_blocks_per_group;	 
	uint32_t	s_frags_per_group;	 
	uint32_t	s_inodes_per_group;	 
	uint32_t	s_mtime;		 
	uint32_t	s_wtime;		 
	uint16_t	s_mnt_count;		 
	int16_t		s_max_mnt_count;	 
	uint16_t	s_magic;		 
	uint16_t	s_state;		 
	uint16_t	s_errors;		 
	uint16_t	s_minor_rev_level;	 
	uint32_t	s_lastcheck;		 
	uint32_t	s_checkinterval;	 
	uint32_t	s_creator_os;		 
	uint32_t	s_rev_level;		 
	uint16_t	s_def_resuid;		 
	uint16_t	s_def_resgid;		 
	uint32_t	s_first_ino;		 
	uint16_t	s_inode_size;		 
	uint16_t	s_block_group_nr;	 
	uint32_t	s_feature_compat;	 
	uint32_t	s_feature_incompat;	 
	uint32_t	s_feature_ro_compat;	 
	uint8_t		s_uuid[16];		 
	char		s_volume_name[16];	 
	char		s_last_mounted[64];	 
	uint32_t	s_algorithm_usage_bitmap;  
	uint8_t		s_prealloc_blocks;	 
	uint8_t		s_prealloc_dir_blocks;	 
	uint16_t	s_reserved_gdt_blocks;	 
 	uint8_t		s_journal_uuid[16];	 
 	uint32_t	s_journal_inum;		 
	uint32_t	s_journal_dev;		 
	uint32_t	s_last_orphan;		 
	uint32_t	s_hash_seed[4];		 
	uint8_t		s_def_hash_version;	 
	uint8_t		s_jnl_backup_type;	 
	uint16_t	s_reserved_word_pad;
 	uint32_t	s_default_mount_opts;
	uint32_t	s_first_meta_bg;	 
	uint32_t	s_mkfs_time;		 
	uint32_t	s_jnl_blocks[17];	 
 	uint32_t	s_blocks_count_hi;	 
	uint32_t	s_r_blocks_count_hi;	 
	uint32_t	s_free_blocks_count_hi;	 
	uint16_t	s_min_extra_isize;	 
	uint16_t	s_want_extra_isize; 	 
	uint32_t	s_flags;		 
	uint16_t	s_raid_stride;		 
	uint16_t	s_mmp_interval;		 
	uint64_t	s_mmp_block;		 
	uint32_t	s_raid_stripe_width;	 
	uint8_t		s_log_groups_per_flex;	 
	uint8_t		s_reserved_char_pad2;
	uint16_t	s_reserved_pad;
	uint32_t	s_reserved[162];	 
};
struct BUG_ext2_super_block {
	char bug[sizeof(struct ext2_super_block) == 1024 ? 1 : -1];
};
#define EXT2_OS_LINUX		0
#define EXT2_OS_HURD		1
#define EXT2_OS_MASIX		2
#define EXT2_OS_FREEBSD		3
#define EXT2_OS_LITES		4
#define EXT2_GOOD_OLD_REV	0	 
#define EXT2_DYNAMIC_REV	1	 
#define EXT2_CURRENT_REV	EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV	EXT2_DYNAMIC_REV
#define EXT2_GOOD_OLD_INODE_SIZE 128
#define EXT3_JNL_BACKUP_BLOCKS	1
#define EXT2_HAS_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_feature_compat & (mask) )
#define EXT2_HAS_RO_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_feature_ro_compat & (mask) )
#define EXT2_HAS_INCOMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->s_feature_incompat & (mask) )
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INO		0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX		0x0020
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004  
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE	0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM		0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK	0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE	0x0040
#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS		0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT		0x0080
#define EXT4_FEATURE_INCOMPAT_MMP		0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200
#define EXT2_FEATURE_COMPAT_SUPP	0
#define EXT2_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED	(~EXT2_FEATURE_INCOMPAT_SUPP)
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED	(~EXT2_FEATURE_RO_COMPAT_SUPP)
#define EXT3_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT3_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT3_FEATURE_INCOMPAT_RECOVER| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT3_FEATURE_INCOMPAT_UNSUPPORTED	(~EXT3_FEATURE_INCOMPAT_SUPP)
#define EXT3_FEATURE_RO_COMPAT_UNSUPPORTED	(~EXT3_FEATURE_RO_COMPAT_SUPP)
#define EXT2_DEF_RESUID		0
#define EXT2_DEF_RESGID		0
#define EXT2_DEFM_DEBUG		0x0001
#define EXT2_DEFM_BSDGROUPS	0x0002
#define EXT2_DEFM_XATTR_USER	0x0004
#define EXT2_DEFM_ACL		0x0008
#define EXT2_DEFM_UID16		0x0010
#define EXT3_DEFM_JMODE		0x0060
#define EXT3_DEFM_JMODE_DATA	0x0020
#define EXT3_DEFM_JMODE_ORDERED	0x0040
#define EXT3_DEFM_JMODE_WBACK	0x0060
#define EXT2_NAME_LEN 255
struct ext2_dir_entry {
	uint32_t	inode;			 
	uint16_t	rec_len;		 
	uint16_t	name_len;		 
	char		name[EXT2_NAME_LEN];	 
};
struct ext2_dir_entry_2 {
	uint32_t	inode;			 
	uint16_t	rec_len;		 
	uint8_t		name_len;		 
	uint8_t		file_type;
	char		name[EXT2_NAME_LEN];	 
};
#define EXT2_FT_UNKNOWN		0
#define EXT2_FT_REG_FILE	1
#define EXT2_FT_DIR		2
#define EXT2_FT_CHRDEV		3
#define EXT2_FT_BLKDEV		4
#define EXT2_FT_FIFO		5
#define EXT2_FT_SOCK		6
#define EXT2_FT_SYMLINK		7
#define EXT2_FT_MAX		8
#define EXT2_DIR_PAD			4
#define EXT2_DIR_ROUND			(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & \
					 ~EXT2_DIR_ROUND)
#endif
