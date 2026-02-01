 

 

#ifndef __C_THREADED_VARIABLES_H
#define __C_THREADED_VARIABLES_H

#include <ncurses_cfg.h>

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#include <curses.h>

extern WINDOW *stdscr_as_function(void);
extern WINDOW *curscr_as_function(void);

extern int LINES_as_function(void);
extern int LINES_as_function(void);
extern int COLS_as_function(void);
extern int TABSIZE_as_function(void);
extern int COLORS_as_function(void);
extern int COLOR_PAIRS_as_function(void);

extern chtype acs_map_as_function(char   );

#endif  
