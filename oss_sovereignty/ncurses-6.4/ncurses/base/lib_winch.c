 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_winch.c,v 1.9 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(chtype)
winch(WINDOW *win)
{
    T((T_CALLED("winch(%p)"), (void *) win));
    if (win != 0) {
	returnChtype((chtype) CharOf(win->_line[win->_cury].text[win->_curx])
		     | AttrOf(win->_line[win->_cury].text[win->_curx]));
    } else {
	returnChtype(0);
    }
}
