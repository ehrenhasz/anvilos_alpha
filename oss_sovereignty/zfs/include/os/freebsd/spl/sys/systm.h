#ifndef _OPENSOLARIS_SYS_SYSTM_H_
#define	_OPENSOLARIS_SYS_SYSTM_H_
#include <sys/endian.h>
#include_next <sys/systm.h>
#include <sys/string.h>
#define	PAGESIZE	PAGE_SIZE
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	PAGEMASK	(~PAGEOFFSET)
#define	delay(x)	pause("soldelay", (x))
#endif	 
