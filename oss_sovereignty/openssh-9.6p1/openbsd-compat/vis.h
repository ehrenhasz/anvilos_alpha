 
 

 

 

#include "includes.h"
#if !defined(HAVE_STRNVIS) || defined(BROKEN_STRNVIS)

#ifndef _VIS_H_
#define	_VIS_H_

#include <sys/types.h>
#include <limits.h>

 
#define	VIS_OCTAL	0x01	 
#define	VIS_CSTYLE	0x02	 

 
#define	VIS_SP		0x04	 
#define	VIS_TAB		0x08	 
#define	VIS_NL		0x10	 
#define	VIS_WHITE	(VIS_SP | VIS_TAB | VIS_NL)
#define	VIS_SAFE	0x20	 
#define	VIS_DQ		0x200	 
#define	VIS_ALL		0x400	 

 
#define	VIS_NOSLASH	0x40	 
#define	VIS_GLOB	0x100	 

 
#define	UNVIS_VALID	 1	 
#define	UNVIS_VALIDPUSH	 2	 
#define	UNVIS_NOCHAR	 3	 
#define	UNVIS_SYNBAD	-1	 
#define	UNVIS_ERROR	-2	 

 
#define	UNVIS_END	1	 

char	*vis(char *, int, int, int);
int	strvis(char *, const char *, int);
int	stravis(char **, const char *, int);
int	strnvis(char *, const char *, size_t, int)
		__attribute__ ((__bounded__(__string__,1,3)));
int	strvisx(char *, const char *, size_t, int)
		__attribute__ ((__bounded__(__string__,1,3)));
int	strunvis(char *, const char *);
int	unvis(char *, char, int *, int);
ssize_t strnunvis(char *, const char *, size_t)
		__attribute__ ((__bounded__(__string__,1,3)));

#endif  

#endif  
