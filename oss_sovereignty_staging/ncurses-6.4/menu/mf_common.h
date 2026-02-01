 

 

 

 

#ifndef MF_COMMON_H_incl
#define MF_COMMON_H_incl 1

#include <ncurses_cfg.h>
#include <curses.h>

#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#if DECL_ERRNO
extern int errno;
#endif

 
#ifdef TRACE
#  ifdef NDEBUG
#    undef NDEBUG
#  endif
#endif

#include <nc_alloc.h>

#if USE_RCS_IDS
#define MODULE_ID(id) static const char Ident[] = id;
#else
#define MODULE_ID(id)		 
#endif

 
#define MAX_REGULAR_CHARACTER (0xff)

#define SET_ERROR(code) (errno=(code))
#define GET_ERROR()     (errno)

#ifdef TRACE
#define RETURN(code)    returnCode( SET_ERROR(code) )
#else
#define RETURN(code)    return( SET_ERROR(code) )
#endif

 
#define _POSTED         (0x01U)	 
#define _IN_DRIVER      (0x02U)	 

#define SetStatus(target,mask) (target)->status |= (unsigned short) (mask)
#define ClrStatus(target,mask) (target)->status = (unsigned short) (target->status & (~mask))

 
#define Call_Hook( object, handler ) \
   if ( (object) != 0 && ((object)->handler) != (void *) 0 )\
   {\
	SetStatus(object, _IN_DRIVER);\
	(object)->handler(object);\
	ClrStatus(object, _IN_DRIVER);\
   }

#endif  
