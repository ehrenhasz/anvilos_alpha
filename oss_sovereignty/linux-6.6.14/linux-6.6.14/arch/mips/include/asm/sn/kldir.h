#ifndef _ASM_SN_KLDIR_H
#define _ASM_SN_KLDIR_H
#define KLDIR_MAGIC		0x434d5f53505f5357
#define KLDIR_OFF_MAGIC			0x00
#define KLDIR_OFF_OFFSET		0x08
#define KLDIR_OFF_POINTER		0x10
#define KLDIR_OFF_SIZE			0x18
#define KLDIR_OFF_COUNT			0x20
#define KLDIR_OFF_STRIDE		0x28
#define KLDIR_ENT_SIZE			0x40
#define KLDIR_MAX_ENTRIES		(0x400 / 0x40)
#ifndef __ASSEMBLY__
typedef struct kldir_ent_s {
	u64		magic;		 
	off_t		offset;		 
	unsigned long	pointer;	 
	size_t		size;		 
	u64		count;		 
	size_t		stride;		 
	char		rsvd[16];	 
} kldir_ent_t;
#endif  
#ifdef CONFIG_SGI_IP27
#include <asm/sn/sn0/kldir.h>
#endif
#endif  
