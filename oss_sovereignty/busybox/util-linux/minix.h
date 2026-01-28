struct minix1_inode {
	uint16_t i_mode;
	uint16_t i_uid;
	uint32_t i_size;
	uint32_t i_time;
	uint8_t  i_gid;
	uint8_t  i_nlinks;
	uint16_t i_zone[9];
};
struct minix2_inode {
	uint16_t i_mode;
	uint16_t i_nlinks;
	uint16_t i_uid;
	uint16_t i_gid;
	uint32_t i_size;
	uint32_t i_atime;
	uint32_t i_mtime;
	uint32_t i_ctime;
	uint32_t i_zone[10];
};
struct minix_superblock {
	uint16_t s_ninodes;
	uint16_t s_nzones;
	uint16_t s_imap_blocks;
	uint16_t s_zmap_blocks;
	uint16_t s_firstdatazone;
	uint16_t s_log_zone_size;
	uint32_t s_max_size;
	uint16_t s_magic;
	uint16_t s_state;
	uint32_t s_zones;
};
struct minix_dir_entry {
	uint16_t inode;
	char name[];
};
#undef BLOCK_SIZE
enum {
	BLOCK_SIZE              = 1024,
	BITS_PER_BLOCK          = BLOCK_SIZE << 3,
	MINIX_ROOT_INO          = 1,
	MINIX_BAD_INO           = 2,
#undef  MINIX1_SUPER_MAGIC
	MINIX1_SUPER_MAGIC      = 0x137F,        
#undef  MINIX1_SUPER_MAGIC2
	MINIX1_SUPER_MAGIC2     = 0x138F,        
#undef  MINIX2_SUPER_MAGIC
	MINIX2_SUPER_MAGIC      = 0x2468,        
#undef  MINIX2_SUPER_MAGIC2
	MINIX2_SUPER_MAGIC2     = 0x2478,        
	MINIX_VALID_FS          = 0x0001,        
	MINIX_ERROR_FS          = 0x0002,        
	INODE_SIZE1             = sizeof(struct minix1_inode),
	INODE_SIZE2             = sizeof(struct minix2_inode),
	MINIX1_INODES_PER_BLOCK = BLOCK_SIZE / sizeof(struct minix1_inode),
	MINIX2_INODES_PER_BLOCK = BLOCK_SIZE / sizeof(struct minix2_inode),
};
