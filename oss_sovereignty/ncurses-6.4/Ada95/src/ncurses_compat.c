 

 

 

 
#include <ncurses_cfg.h>

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#include <curses.h>

#if defined(NCURSES_VERSION_PATCH)

#if NCURSES_VERSION_PATCH < 20081122
extern bool has_mouse(void);
extern int _nc_has_mouse(void);

bool
has_mouse(void)
{
  return (bool)_nc_has_mouse();
}
#endif

 
#if NCURSES_VERSION_PATCH < 20070331
extern bool (is_keypad) (const WINDOW *);
extern bool (is_scrollok) (const WINDOW *);

bool
is_keypad(const WINDOW *win)
{
  return ((win)->_use_keypad);
}

bool
  (is_scrollok) (const WINDOW *win)
{
  return ((win)->_scroll);
}
#endif

#if NCURSES_VERSION_PATCH < 20060107
extern int (getbegx) (WINDOW *);
extern int (getbegy) (WINDOW *);
extern int (getcurx) (WINDOW *);
extern int (getcury) (WINDOW *);
extern int (getmaxx) (WINDOW *);
extern int (getmaxy) (WINDOW *);
extern int (getparx) (WINDOW *);
extern int (getpary) (WINDOW *);

int
  (getbegy) (WINDOW *win)
{
  return ((win) ? (win)->_begy : ERR);
}

int
  (getbegx) (WINDOW *win)
{
  return ((win) ? (win)->_begx : ERR);
}

int
  (getcury) (WINDOW *win)
{
  return ((win) ? (win)->_cury : ERR);
}

int
  (getcurx) (WINDOW *win)
{
  return ((win) ? (win)->_curx : ERR);
}

int
  (getmaxy) (WINDOW *win)
{
  return ((win) ? ((win)->_maxy + 1) : ERR);
}

int
  (getmaxx) (WINDOW *win)
{
  return ((win) ? ((win)->_maxx + 1) : ERR);
}

int
  (getpary) (WINDOW *win)
{
  return ((win) ? (win)->_pary : ERR);
}

int
  (getparx) (WINDOW *win)
{
  return ((win) ? (win)->_parx : ERR);
}
#endif

#endif
