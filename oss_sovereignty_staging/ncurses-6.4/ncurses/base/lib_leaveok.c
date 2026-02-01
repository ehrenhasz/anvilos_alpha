 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_leaveok.c,v 1.7 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
leaveok(WINDOW *win, bool flag)
{
    T((T_CALLED("leaveok(%p,%d)"), (void *) win, flag));

    if (win) {
	win->_leaveok = flag;
	returnCode(OK);
    } else
	returnCode(ERR);
}
