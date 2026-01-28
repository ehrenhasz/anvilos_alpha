

#ifndef _OPENSOLARIS_SYS_DISP_H_
#define	_OPENSOLARIS_SYS_DISP_H_

#include <sys/proc.h>

#define	KPREEMPT_SYNC		(-1)

#define	kpreempt(x)	kern_yield(PRI_USER)

#endif	
