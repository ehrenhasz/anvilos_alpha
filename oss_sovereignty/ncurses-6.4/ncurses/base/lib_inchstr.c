 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_inchstr.c,v 1.17 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
winchnstr(WINDOW *win, chtype *str, int n)
{
    int i = 0;

    T((T_CALLED("winchnstr(%p,%p,%d)"), (void *) win, (void *) str, n));

    if (!win || !str) {
	i = ERR;
    } else {
	int row = win->_cury;
	int col = win->_curx;
	NCURSES_CH_T *text = win->_line[row].text;

	for (; (n < 0 || (i < n)) && (col + i <= win->_maxx); i++) {
	    str[i] = (((chtype) CharOf(text[col + i]) & A_CHARTEXT) |
		      AttrOf(text[col + i]));
	}
	str[i] = (chtype) 0;
    }

    returnCode(i);
}
