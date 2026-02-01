
 

 



#ifndef NCURSES_CPLUS_INTERNAL_H
#define NCURSES_CPLUS_INTERNAL_H 1

#include <ncurses_cfg.h>

#if USE_RCS_IDS
#define MODULE_ID(id) static const char Ident[] = id;
#else
#define MODULE_ID(id)		 
#endif

#if (defined(_WIN32) || defined(_WIN64))
#if defined(EXP_WIN32_DRIVER)
#include <nc_win32.h>
#else
#include <nc_mingw.h>
#endif
#undef KEY_EVENT
#endif

#ifndef _QNX_SOURCE
#include <stdlib.h>
#include <string.h>
#endif

#ifndef CTRL
#define CTRL(x) ((x) & 0x1f)
#endif

#ifndef NULL
#define NULL 0
#endif

#include <nc_string.h>

#endif  
