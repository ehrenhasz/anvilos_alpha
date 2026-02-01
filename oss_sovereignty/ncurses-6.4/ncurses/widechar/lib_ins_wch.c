 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_ins_wch.c,v 1.25 2020/12/05 20:04:59 tom Exp $")

 
NCURSES_EXPORT(int)
_nc_insert_wch(WINDOW *win, const cchar_t *wch)
{
    int cells = _nc_wacs_width(CharOf(CHDEREF(wch)));
    int code = OK;

    if (cells < 0) {
	code = winsch(win, (chtype) CharOf(CHDEREF(wch)));
    } else {
	if (cells == 0)
	    cells = 1;

	if (win->_curx <= win->_maxx) {
	    int cell;
	    struct ldat *line = &(win->_line[win->_cury]);
	    NCURSES_CH_T *end = &(line->text[win->_curx]);
	    NCURSES_CH_T *temp1 = &(line->text[win->_maxx]);
	    NCURSES_CH_T *temp2 = temp1 - cells;

	    CHANGED_TO_EOL(line, win->_curx, win->_maxx);
	    while (temp1 > end)
		*temp1-- = *temp2--;

	    *temp1 = _nc_render(win, *wch);
	    for (cell = 1; cell < cells; ++cell) {
		SetWidecExt(temp1[cell], cell);
	    }

	    win->_curx = (NCURSES_SIZE_T) (win->_curx + cells);
	}
    }
    return code;
}

NCURSES_EXPORT(int)
wins_wch(WINDOW *win, const cchar_t *wch)
{
    int code = ERR;

    T((T_CALLED("wins_wch(%p, %s)"), (void *) win, _tracecchar_t(wch)));

    if (win != 0) {
	NCURSES_SIZE_T oy = win->_cury;
	NCURSES_SIZE_T ox = win->_curx;

	code = _nc_insert_wch(win, wch);

	win->_curx = ox;
	win->_cury = oy;
	_nc_synchook(win);
    }
    returnCode(code);
}

NCURSES_EXPORT(int)
wins_nwstr(WINDOW *win, const wchar_t *wstr, int n)
{
    int code = ERR;

    T((T_CALLED("wins_nwstr(%p,%s,%d)"),
       (void *) win, _nc_viswbufn(wstr, n), n));

    if (win != 0
	&& wstr != 0) {
	if (n < 1)
	    n = INT_MAX;
	code = OK;

	if (n > 0) {
	    const wchar_t *cp;
	    SCREEN *sp = _nc_screen_of(win);
	    NCURSES_SIZE_T oy = win->_cury;
	    NCURSES_SIZE_T ox = win->_curx;

	    for (cp = wstr; (*cp != L'\0') && ((cp - wstr) < n); cp++) {
		int len = _nc_wacs_width(*cp);

		if ((len >= 0 && len != 1) || !is7bits(*cp)) {
		    cchar_t tmp_cchar;
		    wchar_t tmp_wchar = *cp;
		    memset(&tmp_cchar, 0, sizeof(tmp_cchar));
		    (void) setcchar(&tmp_cchar,
				    &tmp_wchar,
				    WA_NORMAL,
				    (short) 0,
				    (void *) 0);
		    code = _nc_insert_wch(win, &tmp_cchar);
		} else {
		     
		    code = _nc_insert_ch(sp, win, (chtype) (*cp));
		}
		if (code != OK)
		    break;
	    }

	    win->_curx = ox;
	    win->_cury = oy;
	    _nc_synchook(win);
	}
    }
    returnCode(code);
}
