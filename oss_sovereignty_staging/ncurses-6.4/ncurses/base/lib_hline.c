 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_hline.c,v 1.16 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
whline(WINDOW *win, chtype ch, int n)
{
    int code = ERR;

    T((T_CALLED("whline(%p,%s,%d)"), (void *) win, _tracechtype(ch), n));

    if (win) {
	struct ldat *line = &(win->_line[win->_cury]);
	NCURSES_CH_T wch;
	int start = win->_curx;
	int end = start + n - 1;

	if (end > win->_maxx)
	    end = win->_maxx;

	CHANGED_RANGE(line, start, end);

	if (ch == 0)
	    SetChar2(wch, ACS_HLINE);
	else
	    SetChar2(wch, ch);
	wch = _nc_render(win, wch);

#if USE_WIDEC_SUPPORT
	if (start > 0 && isWidecExt(line->text[start])) {
	    SetChar2(line->text[start - 1], ' ');
	}
	if (end < win->_maxx && isWidecExt(line->text[end + 1])) {
	    SetChar2(line->text[end + 1], ' ');
	}
#endif
	while (end >= start) {
	    line->text[end] = wch;
	    end--;
	}

	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
