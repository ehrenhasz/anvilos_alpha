
#ifndef UTIL_LINUX_PT_SGI_H
#define UTIL_LINUX_PT_SGI_H

#include <stdint.h>

#define	SGI_LABEL_MAGIC		0x0be5a941

#define SGI_MAXPARTITIONS	16
#define SGI_MAXVOLUMES		15


enum {
	SGI_TYPE_VOLHDR		= 0x00,
	SGI_TYPE_TRKREPL	= 0x01,
	SGI_TYPE_SECREPL	= 0x02,
	SGI_TYPE_SWAP		= 0x03,
	SGI_TYPE_BSD		= 0x04,
	SGI_TYPE_SYSV		= 0x05,
	SGI_TYPE_ENTIRE_DISK	= 0x06,
	SGI_TYPE_EFS		= 0x07,
	SGI_TYPE_LVOL		= 0x08,
	SGI_TYPE_RLVOL		= 0x09,
	SGI_TYPE_XFS		= 0x0a,
	SGI_TYPE_XFSLOG		= 0x0b,
	SGI_TYPE_XLV		= 0x0c,
	SGI_TYPE_XVM		= 0x0d
};

struct sgi_device_parameter {
	unsigned char skew;
	unsigned char gap1;
	unsigned char gap2;
	unsigned char sparecyl;

	uint16_t pcylcount;
	uint16_t head_vol0;
	uint16_t ntrks;		

	unsigned char cmd_tag_queue_depth;
	unsigned char unused0;

	uint16_t unused1;
	uint16_t nsect;		
	uint16_t bytes;
	uint16_t ilfact;
	uint32_t flags;		
	uint32_t datarate;
	uint32_t retries_on_error;
	uint32_t ms_per_word;
	uint16_t xylogics_gap1;
	uint16_t xylogics_syncdelay;
	uint16_t xylogics_readdelay;
	uint16_t xylogics_gap2;
	uint16_t xylogics_readgate;
	uint16_t xylogics_writecont;
} __attribute__((packed));

enum {
	SGI_DEVPARAM_SECTOR_SLIP	= 0x01,
	SGI_DEVPARAM_SECTOR_FWD		= 0x02,
	SGI_DEVPARAM_TRACK_FWD		= 0x04,
	SGI_DEVPARAM_TRACK_MULTIVOL	= 0x08,
	SGI_DEVPARAM_IGNORE_ERRORS	= 0x10,
	SGI_DEVPARAM_RESEEK		= 0x20,
	SGI_DEVPARAM_CMDTAGQ_ENABLE	= 0x40
};


struct sgi_disklabel {
	uint32_t magic;			
	uint16_t root_part_num;		
	uint16_t swap_part_num;		
	unsigned char boot_file[16];	

	struct sgi_device_parameter	devparam;	

	struct sgi_volume {
		unsigned char name[8];	
		uint32_t block_num;	
		uint32_t num_bytes;	
	} __attribute__((packed)) volume[SGI_MAXVOLUMES];

	struct sgi_partition {
		uint32_t num_blocks;	
		uint32_t first_block;	
		uint32_t type;		
	} __attribute__((packed)) partitions[SGI_MAXPARTITIONS];

	
	uint32_t csum;			
	uint32_t padding;		
} __attribute__((packed));

static inline uint32_t sgi_pt_checksum(struct sgi_disklabel *label)
{
	int count;
	uint32_t sum = 0;
	unsigned char *ptr = (unsigned char *) label;

	count = sizeof(*label) / sizeof(uint32_t);
	ptr += sizeof(uint32_t) * (count - 1);

	while (count--) {
		uint32_t val;

		memcpy(&val, ptr, sizeof(uint32_t));
		sum -= be32_to_cpu(val);

		ptr -= sizeof(uint32_t);
	}

	return sum;
}

#endif 
