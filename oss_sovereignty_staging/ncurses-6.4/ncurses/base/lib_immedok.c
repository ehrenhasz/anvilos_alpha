 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_immedok.c,v 1.6 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(void)
immedok(WINDOW *win, bool flag)
{
    T((T_CALLED("immedok(%p,%d)"), (void *) win, flag));

    if (win)
	win->_immed = flag;

    returnVoid;
}
