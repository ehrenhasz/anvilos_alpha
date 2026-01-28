
#ifndef UTIL_LINUX_PT_SUN_H
#define UTIL_LINUX_PT_SUN_H

#include <stdint.h>

#define SUN_LABEL_MAGIC		0xDABE


#define SUN_VTOC_SANITY		0x600DDEEE	
#define SUN_VTOC_VERSION	1
#define SUN_MAXPARTITIONS	8

struct sun_disklabel {
	unsigned char label_id[128];   

	struct sun_vtoc {
		uint32_t version;     
		char	 volume_id[8];
		uint16_t nparts;      

		struct sun_info {        
			uint16_t id;     
			uint16_t flags;  
		} __attribute__ ((packed)) infos[8];

		uint16_t padding;      
		uint32_t bootinfo[3];  
		uint32_t sanity;       
		uint32_t reserved[10]; 
		uint32_t timestamp[8]; 
	} __attribute__ ((packed)) vtoc;

	uint32_t write_reinstruct;     
	uint32_t read_reinstruct;      
	unsigned char spare[148];      
	uint16_t rpm;                  
	uint16_t pcyl;                 
	uint16_t apc;                  
	uint16_t obs1;
	uint16_t obs2;
	uint16_t intrlv;               
	uint16_t ncyl;                 
	uint16_t acyl;                 
	uint16_t nhead;                
	uint16_t nsect;                
	uint16_t obs3;
	uint16_t obs4;

	struct sun_partition {         
		uint32_t start_cylinder;
		uint32_t num_sectors;
	} __attribute__ ((packed)) partitions[8];

	uint16_t magic;                
	uint16_t csum;                 
} __attribute__ ((packed));


#define SUN_TAG_UNASSIGNED	0x00	
#define SUN_TAG_BOOT		0x01	
#define SUN_TAG_ROOT		0x02	
#define SUN_TAG_SWAP		0x03	
#define SUN_TAG_USR		0x04	
#define SUN_TAG_WHOLEDISK	0x05	
#define SUN_TAG_STAND		0x06	
#define SUN_TAG_VAR		0x07	
#define SUN_TAG_HOME		0x08	
#define SUN_TAG_ALTSCTR		0x09	
#define SUN_TAG_CACHE		0x0a	
#define SUN_TAG_RESERVED	0x0b	
#define SUN_TAG_LINUX_SWAP	0x82	
#define SUN_TAG_LINUX_NATIVE	0x83	
#define SUN_TAG_LINUX_LVM	0x8e	
#define SUN_TAG_LINUX_RAID	0xfd	

#define SUN_FLAG_UNMNT		0x01	
#define SUN_FLAG_RONLY		0x10	

static inline uint16_t sun_pt_checksum(const struct sun_disklabel *label)
{
	const uint16_t *ptr = ((const uint16_t *) (label + 1)) - 1;
	uint16_t sum;

	for (sum = 0; ptr >= ((const uint16_t *) label);)
		sum ^= *ptr--;

	return sum;
}

#endif 
