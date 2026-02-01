 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_move.c,v 1.14 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wmove(WINDOW *win, int y, int x)
{
    T((T_CALLED("wmove(%p,%d,%d)"), (void *) win, y, x));

    if (LEGALYX(win, y, x)) {
	win->_curx = (NCURSES_SIZE_T) x;
	win->_cury = (NCURSES_SIZE_T) y;

	win->_flags &= ~_WRAPPED;
	win->_flags |= _HASMOVED;
	returnCode(OK);
    } else
	returnCode(ERR);
}
