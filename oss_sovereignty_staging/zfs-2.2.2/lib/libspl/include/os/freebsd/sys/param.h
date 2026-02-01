 
 

#ifndef _LIBSPL_SYS_PARAM_H
#define	_LIBSPL_SYS_PARAM_H

#include_next <sys/param.h>
#include <unistd.h>

 
#define	MAXNAMELEN	256

#define	UID_NOACCESS	60002		 

#define	MAXUID		UINT32_MAX	 
#define	MAXPROJID	MAXUID		 

#ifdef	PAGESIZE
#undef	PAGESIZE
#endif  

extern size_t spl_pagesize(void);
#define	PAGESIZE	(spl_pagesize())

extern int execvpe(const char *name, char * const argv[], char * const envp[]);

#endif
