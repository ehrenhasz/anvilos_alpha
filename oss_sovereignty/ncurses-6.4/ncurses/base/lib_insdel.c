 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_insdel.c,v 1.14 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
winsdelln(WINDOW *win, int n)
{
    int code = ERR;

    T((T_CALLED("winsdelln(%p,%d)"), (void *) win, n));

    if (win) {
	if (n != 0) {
	    _nc_scroll_window(win, -n, win->_cury, win->_maxy,
			      win->_nc_bkgd);
	    _nc_synchook(win);
	}
	code = OK;
    }
    returnCode(code);
}
