 

 

 

#ifndef CURSES_PRIV_H
#define CURSES_PRIV_H 1

#include <ncurses_dll.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <ncurses_cfg.h>

#if USE_RCS_IDS
#define MODULE_ID(id) static const char Ident[] = id;
#else
#define MODULE_ID(id)		 
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <assert.h>
#include <stdio.h>

#include <errno.h>

#include <curses.h>		 

 
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#define FreeAndNull(p)   do { free(p); p = 0; } while (0)
#define UChar(c)         ((unsigned char)(c))
#define SIZEOF(v)        (sizeof(v) / sizeof(v[0]))

#include <nc_alloc.h>
#include <nc_string.h>

 
#if BROKEN_LINKER || USE_REENTRANT
#define NCURSES_ARRAY(name) \
	NCURSES_WRAPPED_VAR(NCURSES_CONST char * const *, name)

    NCURSES_ARRAY(boolnames);
    NCURSES_ARRAY(boolfnames);
    NCURSES_ARRAY(numnames);
    NCURSES_ARRAY(numfnames);
    NCURSES_ARRAY(strnames);
    NCURSES_ARRAY(strfnames);
#endif

#if NO_LEAKS
    NCURSES_EXPORT(void) _nc_names_leaks(void);
#endif

#ifdef __cplusplus
}
#endif
#endif				 
