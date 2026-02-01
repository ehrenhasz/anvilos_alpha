 

 

#ifndef STRING_HACKS_H
#define STRING_HACKS_H 1

#include <ncurses_cfg.h>

#if HAVE_BSD_STRING_H
#include <bsd/string.h>
#endif

 

#ifdef __cplusplus
#define NCURSES_VOID		 
#else
#define NCURSES_VOID (void)
#endif

#if USE_STRING_HACKS && HAVE_STRLCAT
#define _nc_STRCAT(d,s,n)	NCURSES_VOID strlcat((d),(s),NCURSES_CAST(size_t,n))
#define _nc_STRNCAT(d,s,m,n)	NCURSES_VOID strlcat((d),(s),NCURSES_CAST(size_t,m))
#else
#define _nc_STRCAT(d,s,n)	NCURSES_VOID strcat((d),(s))
#define _nc_STRNCAT(d,s,m,n)	NCURSES_VOID strncat((d),(s),(n))
#endif

#if USE_STRING_HACKS && HAVE_STRLCPY
#define _nc_STRCPY(d,s,n)	NCURSES_VOID strlcpy((d),(s),NCURSES_CAST(size_t,n))
#define _nc_STRNCPY(d,s,n)	NCURSES_VOID strlcpy((d),(s),NCURSES_CAST(size_t,n))
#else
#define _nc_STRCPY(d,s,n)	NCURSES_VOID strcpy((d),(s))
#define _nc_STRNCPY(d,s,n)	NCURSES_VOID strncpy((d),(s),(n))
#endif

#if USE_STRING_HACKS && HAVE_SNPRINTF
#ifdef __cplusplus
#define _nc_SPRINTF             NCURSES_VOID snprintf
#else
#define _nc_SPRINTF             NCURSES_VOID (snprintf)
#endif
#define _nc_SLIMIT(n)           NCURSES_CAST(size_t,n),
#else
#define _nc_SPRINTF             NCURSES_VOID sprintf
#define _nc_SLIMIT(n)		 
#endif

#endif  
