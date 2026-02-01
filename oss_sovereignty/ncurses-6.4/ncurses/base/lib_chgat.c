 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_chgat.c,v 1.13 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wchgat(WINDOW *win,
       int n,
       attr_t attr,
       NCURSES_PAIRS_T pair_arg,
       const void *opts GCC_UNUSED)
{
    int code = ERR;
    int color_pair = pair_arg;

    T((T_CALLED("wchgat(%p,%d,%s,%d)"),
       (void *) win,
       n,
       _traceattr(attr),
       color_pair));

    set_extended_pair(opts, color_pair);
    if (win) {
	struct ldat *line = &(win->_line[win->_cury]);
	int i;

	toggle_attr_on(attr, ColorPair(color_pair));

	for (i = win->_curx; i <= win->_maxx && (n == -1 || (n-- > 0)); i++) {
	    SetAttr(line->text[i], attr);
	    SetPair(line->text[i], color_pair);
	    CHANGED_CELL(line, i);
	}

	code = OK;
    }
    returnCode(code);
}
