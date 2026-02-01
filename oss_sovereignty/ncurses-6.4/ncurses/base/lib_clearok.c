 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_clearok.c,v 1.6 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
clearok(WINDOW *win, bool flag)
{
    T((T_CALLED("clearok(%p,%d)"), (void *) win, flag));

    if (win) {
	win->_clear = flag;
	returnCode(OK);
    } else
	returnCode(ERR);
}
