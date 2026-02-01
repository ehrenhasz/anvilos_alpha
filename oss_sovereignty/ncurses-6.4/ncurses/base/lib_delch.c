 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_delch.c,v 1.14 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wdelch(WINDOW *win)
{
    int code = ERR;

    T((T_CALLED("wdelch(%p)"), (void *) win));

    if (win) {
	NCURSES_CH_T blank = win->_nc_bkgd;
	struct ldat *line = &(win->_line[win->_cury]);
	NCURSES_CH_T *end = &(line->text[win->_maxx]);
	NCURSES_CH_T *temp2 = &(line->text[win->_curx + 1]);
	NCURSES_CH_T *temp1 = temp2 - 1;

	CHANGED_TO_EOL(line, win->_curx, win->_maxx);
	while (temp1 < end)
	    *temp1++ = *temp2++;

	*temp1 = blank;

	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
