


#ifndef _LIBSPL_SYS_PARAM_H
#define	_LIBSPL_SYS_PARAM_H

#include_next <sys/param.h>
#include <unistd.h>


#define	MAXBSIZE	8192
#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		

#define	MAXNAMELEN	256
#define	MAXOFFSET_T	LLONG_MAX

#define	UID_NOBODY	60001		
#define	GID_NOBODY	UID_NOBODY
#define	UID_NOACCESS	60002		

#define	MAXUID		UINT32_MAX	
#define	MAXPROJID	MAXUID		

#ifdef	PAGESIZE
#undef	PAGESIZE
#endif 

extern size_t spl_pagesize(void);
#define	PAGESIZE	(spl_pagesize())

#endif
