
#ifndef __CRAMFS_H
#define __CRAMFS_H

#include <stdint.h>

#define CRAMFS_MAGIC		0x28cd3d45	
#define CRAMFS_SIGNATURE	"Compressed ROMFS"


#define CRAMFS_MODE_WIDTH 16
#define CRAMFS_UID_WIDTH 16
#define CRAMFS_SIZE_WIDTH 24
#define CRAMFS_GID_WIDTH 8
#define CRAMFS_NAMELEN_WIDTH 6
#define CRAMFS_OFFSET_WIDTH 26

#ifndef HOST_IS_BIG_ENDIAN
#define HOST_IS_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#endif


struct cramfs_inode {
	uint32_t mode:16, uid:16;
	
	uint32_t size:24, gid:8;
	
	uint32_t namelen:6, offset:26;
};

struct cramfs_info {
	uint32_t crc;
	uint32_t edition;
	uint32_t blocks;
	uint32_t files;
};


struct cramfs_super {
	uint32_t magic;		
	uint32_t size;		
	uint32_t flags;		
	uint32_t future;		
	uint8_t signature[16];	
	struct cramfs_info fsid;
	uint8_t name[16];		
	struct cramfs_inode root;	
};

#define CRAMFS_FLAG_FSID_VERSION_2	0x00000001	
#define CRAMFS_FLAG_SORTED_DIRS		0x00000002	
#define CRAMFS_FLAG_HOLES		0x00000100	
#define CRAMFS_FLAG_WRONG_SIGNATURE	0x00000200	
#define CRAMFS_FLAG_SHIFTED_ROOT_OFFSET 0x00000400	


#define CRAMFS_SUPPORTED_FLAGS (0xff)


int cramfs_uncompress_block(void *dst, int dstlen, void *src, int srclen);
int cramfs_uncompress_init(void);
int cramfs_uncompress_exit(void);

uint32_t u32_toggle_endianness(int big_endian, uint32_t what);
void super_toggle_endianness(int from_big_endian, struct cramfs_super *super);
void inode_to_host(int from_big_endian, struct cramfs_inode *inode_in,
		   struct cramfs_inode *inode_out);
void inode_from_host(int to_big_endian, struct cramfs_inode *inode_in,
		     struct cramfs_inode *inode_out);

#endif
