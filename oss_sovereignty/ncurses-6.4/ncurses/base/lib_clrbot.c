 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_clrbot.c,v 1.22 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wclrtobot(WINDOW *win)
{
    int code = ERR;

    T((T_CALLED("wclrtobot(%p)"), (void *) win));

    if (win) {
	NCURSES_SIZE_T y;
	NCURSES_SIZE_T startx = win->_curx;
	NCURSES_CH_T blank = win->_nc_bkgd;

	T(("clearing from y = %ld to y = %ld with maxx =  %ld",
	   (long) win->_cury, (long) win->_maxy, (long) win->_maxx));

	for (y = win->_cury; y <= win->_maxy; y++) {
	    struct ldat *line = &(win->_line[y]);
	    NCURSES_CH_T *ptr = &(line->text[startx]);
	    NCURSES_CH_T *end = &(line->text[win->_maxx]);

	    CHANGED_TO_EOL(line, startx, win->_maxx);

	    while (ptr <= end)
		*ptr++ = blank;

	    startx = 0;
	}
	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
