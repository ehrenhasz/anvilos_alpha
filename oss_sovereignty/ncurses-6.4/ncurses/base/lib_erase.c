 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_erase.c,v 1.20 2022/09/03 21:40:27 tom Exp $")

NCURSES_EXPORT(int)
werase(WINDOW *win)
{
    int code = ERR;
    NCURSES_CH_T *start;

    T((T_CALLED("werase(%p)"), (void *) win));

    if (win) {
	NCURSES_CH_T blank;
	NCURSES_CH_T *sp;
	int y;

	blank = win->_nc_bkgd;
	for (y = 0; y <= win->_maxy; y++) {
	    NCURSES_CH_T *end;

	    start = win->_line[y].text;
	    end = &start[win->_maxx];

	     
	    if_WIDEC({
		if (isWidecExt(start[0])) {
		    int x = (win->_parent != 0) ? (win->_begx) : 0;
		    while (x-- > 0) {
			if (isWidecBase(start[-1])) {
			    --start;
			    break;
			}
			--start;
		    }
		}
	    });

	    for (sp = start; sp <= end; sp++)
		*sp = blank;

	    win->_line[y].firstchar = 0;
	    win->_line[y].lastchar = win->_maxx;
	}
	win->_curx = win->_cury = 0;
	win->_flags &= ~_WRAPPED;
	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
