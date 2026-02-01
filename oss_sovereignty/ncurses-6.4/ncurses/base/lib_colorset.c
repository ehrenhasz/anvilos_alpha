 

 

 

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_colorset.c,v 1.16 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wcolor_set(WINDOW *win, NCURSES_PAIRS_T pair_arg, void *opts)
{
    int code = ERR;
    int color_pair = pair_arg;

    T((T_CALLED("wcolor_set(%p,%d)"), (void *) win, color_pair));
    set_extended_pair(opts, color_pair);
    if (win
	&& (SP != 0)
	&& (color_pair >= 0)
	&& (color_pair < SP->_pair_limit)) {
	TR(TRACE_ATTRS, ("... current %ld", (long) GET_WINDOW_PAIR(win)));
	SET_WINDOW_PAIR(win, color_pair);
	if_EXT_COLORS(win->_color = color_pair);
	code = OK;
    }
    returnCode(code);
}
