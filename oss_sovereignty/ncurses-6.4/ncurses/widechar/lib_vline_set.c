 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_vline_set.c,v 1.6 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wvline_set(WINDOW *win, const cchar_t *ch, int n)
{
    int code = ERR;

    T((T_CALLED("wvline(%p,%s,%d)"), (void *) win, _tracecchar_t(ch), n));

    if (win) {
	NCURSES_CH_T wch;
	int row = win->_cury;
	int col = win->_curx;
	int end = row + n - 1;

	if (end > win->_maxy)
	    end = win->_maxy;

	if (ch == 0)
	    wch = *WACS_VLINE;
	else
	    wch = *ch;
	wch = _nc_render(win, wch);

	while (end >= row) {
	    struct ldat *line = &(win->_line[end]);
	    line->text[col] = wch;
	    CHANGED_CELL(line, col);
	    end--;
	}

	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
