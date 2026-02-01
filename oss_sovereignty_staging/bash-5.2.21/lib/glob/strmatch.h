 

#ifndef	_STRMATCH_H
#define	_STRMATCH_H	1

#include <config.h>

#include "stdc.h"

 
#undef  FNM_PATHNAME
#undef  FNM_NOESCAPE
#undef  FNM_PERIOD

 

 
#define	FNM_PATHNAME	(1 << 0)  
#define	FNM_NOESCAPE	(1 << 1)  
#define	FNM_PERIOD	(1 << 2)  

 
#undef FNM_LEADING_DIR
#undef FNM_CASEFOLD
#undef FNM_EXTMATCH

#define FNM_LEADING_DIR	(1 << 3)  
#define FNM_CASEFOLD	(1 << 4)  
#define FNM_EXTMATCH	(1 << 5)  

#define FNM_FIRSTCHAR	(1 << 6)  
#define FNM_DOTDOT	(1 << 7)  

 
#undef FNM_NOMATCH

#define	FNM_NOMATCH	1

 
extern int strmatch PARAMS((char *, char *, int));

#if HANDLE_MULTIBYTE
extern int wcsmatch PARAMS((wchar_t *, wchar_t *, int));
#endif

#endif  
