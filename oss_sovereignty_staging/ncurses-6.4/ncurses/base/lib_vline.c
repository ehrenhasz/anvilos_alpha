 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_vline.c,v 1.15 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wvline(WINDOW *win, chtype ch, int n)
{
    int code = ERR;

    T((T_CALLED("wvline(%p,%s,%d)"), (void *) win, _tracechtype(ch), n));

    if (win) {
	NCURSES_CH_T wch;
	int row = win->_cury;
	int col = win->_curx;
	int end = row + n - 1;

	if (end > win->_maxy)
	    end = win->_maxy;

	if (ch == 0)
	    SetChar2(wch, ACS_VLINE);
	else
	    SetChar2(wch, ch);
	wch = _nc_render(win, wch);

	while (end >= row) {
	    struct ldat *line = &(win->_line[end]);
#if USE_WIDEC_SUPPORT
	    if (col > 0 && isWidecExt(line->text[col])) {
		SetChar2(line->text[col - 1], ' ');
	    }
	    if (col < win->_maxx && isWidecExt(line->text[col + 1])) {
		SetChar2(line->text[col + 1], ' ');
	    }
#endif
	    line->text[col] = wch;
	    CHANGED_CELL(line, col);
	    end--;
	}

	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
