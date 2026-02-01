 

 

 

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_wattron.c,v 1.13 2022/04/15 22:34:38 tom Exp $")

NCURSES_EXPORT(int)
wattr_on(WINDOW *win, attr_t at, void *opts GCC_UNUSED)
{
    T((T_CALLED("wattr_on(%p,%s)"), (void *) win, _traceattr(at)));
    if (win != 0) {
	T(("... current %s (%d)",
	   _traceattr(WINDOW_ATTRS(win)),
	   GET_WINDOW_PAIR(win)));

	if_EXT_COLORS({
	    if (at & A_COLOR) {
		win->_color = PairNumber(at);
		set_extended_pair(opts, win->_color);
	    }
	});
	toggle_attr_on(WINDOW_ATTRS(win), at);
	returnCode(OK);
    } else
	returnCode(ERR);
}
