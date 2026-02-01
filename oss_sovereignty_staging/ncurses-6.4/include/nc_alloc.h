 

 
 

#ifndef NC_ALLOC_included
#define NC_ALLOC_included 1
 

#include <ncurses_cfg.h>
#include <curses.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(HAVE_LIBDMALLOC) && HAVE_LIBDMALLOC
#include <string.h>
#undef strndup		 
#include <dmalloc.h>     
#else
#undef  HAVE_LIBDMALLOC
#define HAVE_LIBDMALLOC 0
#endif

#if defined(HAVE_LIBDBMALLOC) && HAVE_LIBDBMALLOC
#include <dbmalloc.h>    
#else
#undef  HAVE_LIBDBMALLOC
#define HAVE_LIBDBMALLOC 0
#endif

#if defined(HAVE_LIBMPATROL) && HAVE_LIBMPATROL
#include <mpatrol.h>     
#else
#undef  HAVE_LIBMPATROL
#define HAVE_LIBMPATROL 0
#endif

#ifndef NO_LEAKS
#define NO_LEAKS 0
#endif

#if HAVE_LIBDBMALLOC || HAVE_LIBDMALLOC || NO_LEAKS
#define HAVE_NC_FREEALL 1
struct termtype;
extern GCC_NORETURN  NCURSES_EXPORT(void) _nc_free_tinfo(int) GCC_DEPRECATED("use exit_terminfo");

#ifdef NCURSES_INTERNALS
extern GCC_NORETURN NCURSES_EXPORT(void) _nc_free_tic(int);
extern void _nc_leaks_dump_entry(void);
extern NCURSES_EXPORT(void) _nc_leaks_tic(void);

#if NCURSES_SP_FUNCS
extern GCC_NORETURN NCURSES_EXPORT(void) NCURSES_SP_NAME(_nc_free_and_exit)(SCREEN*, int);
#endif
extern GCC_NORETURN NCURSES_EXPORT(void) _nc_free_and_exit(int);

#else  
extern GCC_NORETURN NCURSES_EXPORT(void) _nc_free_and_exit(int) GCC_DEPRECATED("use exit_curses");
#endif

#define ExitProgram(code) exit_curses(code)

#else
extern GCC_NORETURN NCURSES_EXPORT(void) _nc_free_and_exit(int) GCC_DEPRECATED("use exit_curses");
#endif  

#ifndef HAVE_NC_FREEALL
#define HAVE_NC_FREEALL 0
#endif

#ifndef ExitProgram
#define ExitProgram(code) exit(code)
#endif

 
extern NCURSES_EXPORT(void *) _nc_doalloc(void *, size_t);
#if !HAVE_STRDUP
#undef strdup
#define strdup _nc_strdup
extern NCURSES_EXPORT(char *) _nc_strdup(const char *);
#endif

 
extern NCURSES_EXPORT(void) _nc_leaks_tinfo(void);

#define typeMalloc(type,elts) (type *)malloc((size_t)(elts)*sizeof(type))
#define typeCalloc(type,elts) (type *)calloc((size_t)(elts),sizeof(type))
#define typeRealloc(type,elts,ptr) (type *)_nc_doalloc(ptr, (size_t)(elts)*sizeof(type))

#ifdef __cplusplus
}
#endif

 

#endif  
