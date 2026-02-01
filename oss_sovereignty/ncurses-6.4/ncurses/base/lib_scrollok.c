 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_scrollok.c,v 1.6 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
scrollok(WINDOW *win, bool flag)
{
    T((T_CALLED("scrollok(%p,%d)"), (void *) win, flag));

    if (win) {
	win->_scroll = flag;
	returnCode(OK);
    } else
	returnCode(ERR);
}
