 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_hline_set.c,v 1.6 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
whline_set(WINDOW *win, const cchar_t *ch, int n)
{
    int code = ERR;

    T((T_CALLED("whline_set(%p,%s,%d)"), (void *) win, _tracecchar_t(ch), n));

    if (win) {
	struct ldat *line = &(win->_line[win->_cury]);
	NCURSES_CH_T wch;
	int start = win->_curx;
	int end = start + n - 1;

	if (end > win->_maxx)
	    end = win->_maxx;

	CHANGED_RANGE(line, start, end);

	if (ch == 0)
	    wch = *WACS_HLINE;
	else
	    wch = *ch;
	wch = _nc_render(win, wch);

	while (end >= start) {
	    line->text[end] = wch;
	    end--;
	}

	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
