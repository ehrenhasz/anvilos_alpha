 
 

 

 

#ifndef HAVE_FNMATCH_H
 
#define __BSD_VISIBLE 1

#ifndef	_FNMATCH_H_
#define	_FNMATCH_H_

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#define	FNM_NOMATCH	1	 
#define	FNM_NOSYS	2	 

#define	FNM_NOESCAPE	0x01	 
#define	FNM_PATHNAME	0x02	 
#define	FNM_PERIOD	0x04	 
#if __BSD_VISIBLE
#define	FNM_LEADING_DIR	0x08	 
#define	FNM_CASEFOLD	0x10	 
#define	FNM_IGNORECASE	FNM_CASEFOLD
#define	FNM_FILE_NAME	FNM_PATHNAME
#endif

 
int	 fnmatch(const char *, const char *, int);
 

#endif  
#endif  
