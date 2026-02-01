 

 

#ifndef NC_ACCESS_included
#define NC_ACCESS_included 1
 

#include <ncurses_cfg.h>
#include <curses.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

 
#ifdef USE_ROOT_ENVIRON

#define use_terminfo_vars() 1

#else

#define use_terminfo_vars() _nc_env_access()
extern NCURSES_EXPORT(int) _nc_env_access (void);

#endif

 
#ifdef USE_ROOT_ACCESS

#define safe_fopen(name,mode) fopen(name,mode)
#define safe_open3(name,flags,mode) open(name,flags,mode)

#else

#define safe_fopen(name,mode) fopen(name,mode)
#define safe_open3(name,flags,mode) open(name,flags,mode)
extern NCURSES_EXPORT(FILE *) _nc_safe_fopen (const char *, const char *);
extern NCURSES_EXPORT(int) _nc_safe_open3 (const char *, int, mode_t);

#endif

#ifdef __cplusplus
}
#endif

 

#endif  
