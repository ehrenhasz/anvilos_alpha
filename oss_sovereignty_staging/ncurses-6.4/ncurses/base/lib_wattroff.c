 

 

 

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_wattroff.c,v 1.11 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wattr_off(WINDOW *win, attr_t at, void *opts GCC_UNUSED)
{
    T((T_CALLED("wattr_off(%p,%s)"), (void *) win, _traceattr(at)));
    if (win) {
	T(("... current %s (%d)",
	   _traceattr(WINDOW_ATTRS(win)),
	   GET_WINDOW_PAIR(win)));

	if_EXT_COLORS({
	    if (at & A_COLOR)
		win->_color = 0;
	});
	toggle_attr_off(WINDOW_ATTRS(win), at);
	returnCode(OK);
    } else
	returnCode(ERR);
}
