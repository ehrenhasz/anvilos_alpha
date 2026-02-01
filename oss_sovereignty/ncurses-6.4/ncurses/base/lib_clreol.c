 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_clreol.c,v 1.24 2021/10/23 19:06:01 tom Exp $")

NCURSES_EXPORT(int)
wclrtoeol(WINDOW *win)
{
    int code = ERR;

    T((T_CALLED("wclrtoeol(%p)"), (void *) win));

    if (win) {
	NCURSES_CH_T blank;
	NCURSES_CH_T *ptr, *end;
	struct ldat *line;
	NCURSES_SIZE_T y = win->_cury;
	NCURSES_SIZE_T x = win->_curx;

	 
	if (IS_WRAPPED(win) != 0
	    && y < win->_maxy) {
	    win->_flags &= ~_WRAPPED;
	}

	 
	if (IS_WRAPPED(win)
	    || y > win->_maxy
	    || x > win->_maxx)
	    returnCode(ERR);

	blank = win->_nc_bkgd;
	line = &win->_line[y];
	CHANGED_TO_EOL(line, x, win->_maxx);

	ptr = &(line->text[x]);
	end = &(line->text[win->_maxx]);

	while (ptr <= end)
	    *ptr++ = blank;

	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
