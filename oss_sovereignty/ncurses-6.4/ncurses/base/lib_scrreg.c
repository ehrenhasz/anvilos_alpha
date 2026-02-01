 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_scrreg.c,v 1.12 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wsetscrreg(WINDOW *win, int top, int bottom)
{
    T((T_CALLED("wsetscrreg(%p,%d,%d)"), (void *) win, top, bottom));

    if (win &&
	top >= 0 && top <= win->_maxy &&
	bottom >= 0 && bottom <= win->_maxy &&
	bottom > top) {
	win->_regtop = (NCURSES_SIZE_T) top;
	win->_regbottom = (NCURSES_SIZE_T) bottom;

	returnCode(OK);
    } else
	returnCode(ERR);
}
