 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_clear.c,v 1.9 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wclear(WINDOW *win)
{
    int code = ERR;

    T((T_CALLED("wclear(%p)"), (void *) win));

    if ((code = werase(win)) != ERR)
	win->_clear = TRUE;

    returnCode(code);
}
