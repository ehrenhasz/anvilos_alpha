 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_in_wchnstr.c,v 1.9 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
win_wchnstr(WINDOW *win, cchar_t *wchstr, int n)
{
    int code = OK;

    T((T_CALLED("win_wchnstr(%p,%p,%d)"), (void *) win, (void *) wchstr, n));
    if (win != 0
	&& wchstr != 0) {
	NCURSES_CH_T *src;
	int row, col;
	int j, k, limit;

	getyx(win, row, col);
	limit = getmaxx(win) - col;
	src = &(win->_line[row].text[col]);

	if (n < 0) {
	    n = limit;
	} else if (n > limit) {
	    n = limit;
	}
	for (j = k = 0; j < n; ++j) {
	    if (j == 0 || !WidecExt(src[j]) || isWidecBase(src[j])) {
		wchstr[k++] = src[j];
	    }
	}
	memset(&(wchstr[k]), 0, sizeof(*wchstr));
	T(("result = %s", _nc_viscbuf(wchstr, n)));
    } else {
	code = ERR;
    }
    returnCode(code);
}
