 

 

 

#ifndef NEW_PAIR_H
#define NEW_PAIR_H 1
 

#include <ncurses_cfg.h>
#include <ncurses_dll.h>

#include <sys/types.h>

#undef SCREEN
#define SCREEN struct screen
SCREEN;

#define LIMIT_TYPED(n,t) \
	(t)(((n) > MAX_OF_TYPE(t)) \
	    ? MAX_OF_TYPE(t) \
	    : ((n) < -MAX_OF_TYPE(t)) \
	       ? -MAX_OF_TYPE(t) \
	       : (n))

#define limit_COLOR(n) LIMIT_TYPED(n,NCURSES_COLOR_T)
#define limit_PAIRS(n) LIMIT_TYPED(n,NCURSES_PAIRS_T)

#define MAX_XCURSES_PAIR MAX_OF_TYPE(NCURSES_PAIRS_T)

#if NCURSES_EXT_COLORS
#define OPTIONAL_PAIR	GCC_UNUSED
#define get_extended_pair(opts, color_pair) \
	if ((opts) != NULL) { \
	    *(int*)(opts) = color_pair; \
	}
#define set_extended_pair(opts, color_pair) \
	if ((opts) != NULL) { \
	    color_pair = *(const int*)(opts); \
	}
#else
#define OPTIONAL_PAIR	 
#define get_extended_pair(opts, color_pair)  
#define set_extended_pair(opts, color_pair) \
	if ((opts) != NULL) { \
	    color_pair = -1; \
	}
#endif

#ifdef NEW_PAIR_INTERNAL

typedef enum {
    cpKEEP = -1,		 
    cpFREE = 0,			 
    cpINIT = 1			 
} CPMODE;

typedef struct _color_pairs
{
    int fg;
    int bg;
#if NCURSES_EXT_COLORS
    int mode;			 
    int prev;			 
    int next;			 
#endif
}
colorpair_t;

#define MakeColorPair(target,f,b) target.fg = f, target.bg = b
#define isSamePair(a,b)		((a).fg == (b).fg && (a).bg == (b).bg)
#define FORE_OF(c)		(c).fg
#define BACK_OF(c)		(c).bg

 
#define ValidPair(sp,pair) \
    ((sp != 0) && (pair >= 0) && (pair < sp->_pair_limit) && sp->_coloron)

#if NCURSES_EXT_COLORS
extern NCURSES_EXPORT(void)     _nc_copy_pairs(SCREEN*, colorpair_t*, colorpair_t*, int);
extern NCURSES_EXPORT(void)     _nc_free_ordered_pairs(SCREEN*);
extern NCURSES_EXPORT(void)     _nc_reset_color_pair(SCREEN*, int, colorpair_t*);
extern NCURSES_EXPORT(void)     _nc_set_color_pair(SCREEN*, int, int);
#else
#define _nc_free_ordered_pairs(sp)  
#define _nc_reset_color_pair(sp, pair, data)  
#define _nc_set_color_pair(sp, pair, mode)  
#endif

#else

typedef struct _color_pairs colorpair_t;

#endif  

#if NO_LEAKS
extern NCURSES_EXPORT(void)     _nc_new_pair_leaks(SCREEN*);
#endif

 

#endif  
