#ifndef _XTENSA_BOOTPARAM_H
#define _XTENSA_BOOTPARAM_H
#define BP_VERSION 0x0001
#define BP_TAG_COMMAND_LINE	0x1001	 
#define BP_TAG_INITRD		0x1002	 
#define BP_TAG_MEMORY		0x1003	 
#define BP_TAG_SERIAL_BAUDRATE	0x1004	 
#define BP_TAG_SERIAL_PORT	0x1005	 
#define BP_TAG_FDT		0x1006	 
#define BP_TAG_FIRST		0x7B0B   
#define BP_TAG_LAST 		0x7E0B	 
#ifndef __ASSEMBLY__
typedef struct bp_tag {
	unsigned short id;	 
	unsigned short size;	 
	unsigned long data[];	 
} bp_tag_t;
struct bp_meminfo {
	unsigned long type;
	unsigned long start;
	unsigned long end;
};
#define MEMORY_TYPE_CONVENTIONAL	0x1000
#define MEMORY_TYPE_NONE		0x2000
#endif
#endif
